#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cmath>
#include "exedit.hpp"
#include "philox4x32.hpp"

constexpr int track_n = 1;
constexpr int track_hz_scale = 100;
inline static char name[] = "単純音生成";
inline static char track_name1[] = "Hz";
inline static char* track_name[track_n] = { track_name1 };
inline static int track_default[track_n] = { 44000 };
inline static int track_s[track_n] = { 1375 };
inline static int track_e[track_n] = { 2500000 };
inline static int track_scale[track_n] = { track_hz_scale };
inline static int track_drag_min[track_n] = { 5500 };
inline static int track_drag_max[track_n] = { 352000 };

constexpr int check_n = 15;
constexpr int type_n = 7;
static char* check_name[check_n] = { const_cast<char*>("三角波\0のこぎり波\0正弦波\0矩形波\0パルス波1/4\0パルス波1/8\0ホワイトノイズ\0"),
                                    const_cast<char*>("/2"),
                                    const_cast<char*>("C"),
                                    const_cast<char*>("C#"),
                                    const_cast<char*>("D"),
                                    const_cast<char*>("D#"),
                                    const_cast<char*>("E"),
                                    const_cast<char*>("F"),
                                    const_cast<char*>("F#"),
                                    const_cast<char*>("G"),
                                    const_cast<char*>("G#"),
                                    const_cast<char*>("A"),
                                    const_cast<char*>("A#"),
                                    const_cast<char*>("B"),
                                    const_cast<char*>("*2")
};
static int check_default[check_n] = { -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };


struct Exdata {
    int type;
    float last_rate;
};

static char exdata_def[sizeof(Exdata)] = { 0,0,0,0, 0,0,0,0 };
static ExEdit::ExdataUse exdata_use[] = {
    {
        .type = ExEdit::ExdataUse::Type::Number,
        .size = 4,
        .name = "type",
    },
    {
        .type = ExEdit::ExdataUse::Type::Number,
        .size = 4,
        .name = "last_rate",
    },
};


float preset_scale[12];

intptr_t get_func_address(intptr_t call_address) {
    return 4 + call_address + *reinterpret_cast<ptrdiff_t*>(call_address);
}
static void(__cdecl* update_any_exdata)(ExEdit::ObjectFilterIndex, const char*) = nullptr;
static void*(__cdecl* get_or_create_cache)(ExEdit::ObjectFilterIndex ofi, int w, int h, int bitcount, int v_func_id, int* old_cache_exists) = nullptr;



short triangle(double rate) {
    int r;
    if (rate <= 0.25) {
        r = (int)round(rate * 4.0 * 32767.0);
    } else if (0.75 <= rate) {
        r = (int)round((rate - 0.75) * 4.0 * 32768.0) - 0x8000;
    } else {
        r = 0x7fff - (int)round((rate - 0.25) * 2.0 * 65535.0);
    }
    return (short)r;
}
short sawtooth(double rate) {
    return (short)round(rate * 65535.0) - 0x8000;
}
short sine(double rate) {
    return (short)round(sin(rate * 6.283185307179586476925286766559) * 32767.0);
}
short square(double rate) {
    if (rate < 0.5) {
        return SHRT_MAX;
    } else {
        return SHRT_MIN;
    }
}
short pulse_quarter(double rate) {
    if (rate < 0.25) {
        return SHRT_MAX;
    } else {
        return SHRT_MIN;
    }
}
short pulse_one_eighth(double rate) {
    if (rate < 0.125) {
        return SHRT_MAX;
    } else {
        return SHRT_MIN;
    }
}
struct noise_state {
    short last_value; // 最後に生成した乱数値．
    int waves; // 生成した乱数の個数．
};
struct {
    noise_state state;
    double step;
    my_rng::philox4x32 rng;

    void recall(int seed, noise_state const& state) {
        this->state = state;
        rng.seed(my_rng::philox4x32::default_seed + seed);
        rng.discard(state.waves);
    }
    void move_next() {
        state.waves++;
        state.last_value = static_cast<short>(rng() >> 16);
    }
    short value() const { return state.last_value; }
} noise_generator{};
short white_noise(double rate) {
    constexpr int noise_freq = 8; // 1周期当たりに発生する乱数の個数．
    if (rate - std::floor(rate * noise_freq) / noise_freq < noise_generator.step)
        noise_generator.move_next();
    return noise_generator.value();
}

BOOL func_proc(ExEdit::Filter* efp, ExEdit::FilterProcInfo* efpip) {
    Exdata* exdata = reinterpret_cast<Exdata*>(efp->exdata_ptr);

    double rate = (double)exdata->last_rate;
    if (rate < 0.0) {
        rate += ceil(-rate);
    } else if (1.0 <= rate) {
        rate -= floor(rate);
    }

    double hz = (double)efp->track[0] * (1.0 / (double)track_hz_scale);
    if (efpip->audio_speed) {
        hz *= (double)efpip->audio_speed * 0.000001;

        // double frame = (double)efpip->audio_milliframe * 0.001 + (double)(efpip->frame - efpip->frame_num);
        if (0 < efpip->audio_speed && efpip->audio_milliframe == 0) {
            rate = 0.0;
        }
    } else {
        if (efpip->frame == 0) { // 1フレーム目
            rate = 0.0;
        }
    }
    double step = hz / (double)efpip->audio_rate;


    // 乱数位置を記録するためのキャッシュ．
    struct {
        int frame;
        noise_state states[2];
    }* cache = nullptr;
    if (exdata->type == 6) {
        int cache_exists_flag;
        cache = reinterpret_cast<decltype(cache)>(get_or_create_cache(
            efp->processing, sizeof(*cache) / sizeof(short), 1, 8 * sizeof(short),
            reinterpret_cast<int>(&white_noise), &cache_exists_flag));

        if (cache != nullptr) {
            int const frame = efpip->frame + efpip->add_frame;
            if (frame == 0 || frame < cache->frame || cache_exists_flag == 0)
                // 新規キャッシュ，あるいは時間が巻き戻っているなら初期化．
                std::memset(cache, 0, 2 * sizeof(*cache));
            else if (cache->frame == frame)
                // 同一フレームなので音声ソースの更新．読み込み元を巻き戻す．
                cache->states[0] = cache->states[1];
            else cache->states[1] = cache->states[0];
            cache->frame = frame;

            noise_generator.recall(static_cast<int>(efp->processing), cache->states[0]);
        }
        noise_generator.step = step;
    }

    short(__cdecl * func)(double rate);
    switch (exdata->type) {
    case 0: {
        (func) = triangle;
    }break;
    case 1: {
        (func) = sawtooth;
    }break;
    case 2: {
        (func) = sine;
    }break;
    case 3: {
        (func) = square;
    }break;
    case 4: {
        (func) = pulse_quarter;
    }break;
    case 5: {
        (func) = pulse_one_eighth;
    }break;
    case 6: {
        (func) = white_noise;
    }break;
    default: {
        return FALSE;
    }
    }
    if (efpip->audio_ch == 2) {
        for (int i = 0; i < efpip->audio_n; i++) {
            efpip->audio_data[i * 2] = efpip->audio_data[i * 2 + 1] = func(rate);
            rate += step;
            if (1.0 <= rate) {
                rate -= 1.0;
            }
        }
    } else {
        for (int i = 0; i < efpip->audio_n; i++) {
            efpip->audio_data[i] = func(rate);
            rate += step;
            if (1.0 <= rate) {
                rate -= 1.0;
            }
        }
    }
    exdata->last_rate = (float)rate;
    if (cache != nullptr) cache->states[0] = noise_generator.state;


    return TRUE;
}
BOOL func_init(ExEdit::Filter* efp) {
    (update_any_exdata) = reinterpret_cast<decltype(update_any_exdata)>((int)efp->exedit_fp->dll_hinst + 0x4a7e0);
    (get_or_create_cache) = reinterpret_cast<decltype(get_or_create_cache)>(get_func_address((int)efp->exedit_fp->dll_hinst + 0x1c1ea));

    for (int i = 0; i < 9; i++) {
        preset_scale[i] = 1375.0 * pow(2.0, (double)(i + 3) / 12.0);
    }
    for (int i = 9; i < 12; i++) {
        preset_scale[i] = 1375.0 * pow(2.0, (double)(i - 9) / 12.0);
    }
    return TRUE;
}
void update_extendedfilter_wnd(ExEdit::Filter* efp) {
    Exdata* exdata = reinterpret_cast<Exdata*>(efp->exdata_ptr);
    if (exdata->type < 0 || type_n <= exdata->type) return;

    SendMessageA(efp->exfunc->get_hwnd(efp->processing, 6, 0), CB_SETCURSEL, exdata->type, 0);
}

BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, AviUtl::EditHandle* editp, ExEdit::Filter* efp) {
    if (message == ExEdit::ExtendedFilter::Message::WM_EXTENDEDFILTER_COMMAND) {
        if (LOWORD(wparam) == ExEdit::ExtendedFilter::CommandId::EXTENDEDFILTER_SELECT_DROPDOWN) {
            int type = std::clamp((int)lparam, 0, type_n - 1);
            Exdata* exdata = reinterpret_cast<Exdata*>(efp->exdata_ptr);
            if (exdata->type != type) {
                efp->exfunc->set_undo(efp->processing, 0);
                exdata->type = type;
                update_any_exdata(efp->processing, exdata_use[0].name);
                update_extendedfilter_wnd(efp);
            }
            return TRUE;
        }
        if (LOWORD(wparam) == ExEdit::ExtendedFilter::CommandId::EXTENDEDFILTER_PUSH_BUTTON) {
            int newval = efp->track_value_left[0];
            if (HIWORD(wparam) == 1) {
                if (efp->track_s[0] * 2 <= efp->track_value_left[0]) {
                    newval /= 2;
                }
            } else if (HIWORD(wparam) == 14) {
                if (efp->track_value_left[0] * 2 <= efp->track_e[0]) {
                    newval *= 2;
                }
            } else {
                int sc = HIWORD(wparam) - 2;
                if ((float)efp->track_value_left[0] <= preset_scale[sc]) {
                    newval = (int)round(preset_scale[sc]);
                } else {
                    double newvald = (double)preset_scale[sc];
                    double val;
                    for (val = preset_scale[sc] * 2.0; val <= (double)efp->track_e[0]; val *= 2.0) {
                        if ((double)efp->track_value_left[0] < val) {
                            break;
                        }
                        newvald = val;
                    }
                    if (val - efp->track_value_left[0] < efp->track_value_left[0] - newvald) {
                        newvald = val;
                    }
                    newval = (int)round(newvald);
                }
            }
            if (efp->track_value_left[0] != newval) {
                efp->exfunc->set_undo(efp->processing, 0);
                efp->track_value_left[0] = newval;
                efp->exfunc->x00(efp->processing);
            }
            return TRUE;
        }
    }
    return FALSE;
}

int func_window_init(HINSTANCE hinstance, HWND hwnd, int y, int base_id, int sw_param, ExEdit::Filter* efp) {
    if (sw_param) {
        update_extendedfilter_wnd(efp);
        HWND ctrlhwnd;
        for (int i = 0; i < 15; i++) {
            ctrlhwnd = efp->exfunc->get_hwnd(efp->processing, 7, i);
            if (ctrlhwnd) {
                SetWindowPos(ctrlhwnd, (HWND)0, 0, y - 20 - (21 * 14), 0, 0, SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        for (int i = 1; i < 15; i++) {
            ctrlhwnd = efp->exfunc->get_hwnd(efp->processing, 4, i);
            if (ctrlhwnd) {
                SetWindowPos(ctrlhwnd, (HWND)0, i * 22 + 148, y - 20 - (21 * 14), 20, 16, SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
    }
    return -21 * 14;
}

ExEdit::Filter ef = {
    .flag = ExEdit::Filter::Flag::Audio | ExEdit::Filter::Flag::Input,
    .name = name,
    .track_n = track_n,
    .track_name = track_name,
    .track_default = track_default,
    .track_s = track_s,
    .track_e = track_e,
    .check_n = check_n,
    .check_name = check_name,
    .check_default = check_default,
    .func_proc = &func_proc,
    .func_init = &func_init,
    .func_WndProc = &func_WndProc,
    .exdata_size = sizeof(Exdata),
    .func_window_init = &func_window_init,
    .exdata_def = exdata_def,
    .exdata_use = exdata_use,
    .track_scale = track_scale,
    .track_drag_min = track_drag_min,
    .track_drag_max = track_drag_max,
};

ExEdit::Filter* filter_list[] = {
    &ef,
    NULL
};

EXTERN_C __declspec(dllexport)ExEdit::Filter** __stdcall GetFilterTableList() {
    return filter_list;
}
