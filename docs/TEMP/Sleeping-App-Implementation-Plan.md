StudyBud: Screen UI Specifications & Implementation GuideThis document contains the complete structural breakdown, UI layout specs, and technical implementation details for the StudyBud Design and Technology HSC Major Work project. All screens are designed specifically for the Waveshare ESP32-S3-LCD-2.8C round 480×480 display using LVGL v8.2.0 in C.  Architecture & Design System OverviewDisplay Dimensions: 480 × 480 pixels (Round TFT Display, ST7701S driver).  Input Device: Rotary encoder with push-button.  Typography: Nunito font family (font_12, font_16, font_20_bold, font_36).  Color Palette: Muted Lavender & Sage palette (WCAG AA compliant).  Global Transition: Soft fade-in / fade-out (LV_SCR_LOAD_ANIM_FADE_ON) across screen switches to remain non-intrusive in low-light workspace environments.  1. Module: Breathing App                 [ SCREEN 1: WELCOME SCREEN ]
              /===============================\
             |         WELCOME BACK!           |
             |          .---------.            |
             |         /   BEGIN   \           |  <-- Primary Circular Button
             |         \  BUTTON   /           |      (Scales up when focused)
             |          '---------'            |
             |            .-----.              |
             |           (   3   )             |  <-- Secondary Circular Button
             |            '-----'              |      (Cycles 1–40; grows when focused)
              \===============================/
                             |  (Click BEGIN)
                             v
               [ SCREEN 2: INSTRUCTION SCREEN ]
              /===============================\
             |      It's your 3rd time        |
             |            today!              |  <-- Usage Acknowledgment
             |     Take a moment for          |
             |     yourself...                |  <-- Encouraging Message
             |  [Instructions & abort note]   |  <-- Small Instruction Subtext
             |   Press encoder to begin...    |  <-- Prompt Trigger
              \===============================/
                             |  (Click Encoder)
                             v
               [ SCREEN 3: ACTIVE BREATH LOOP ]
              /===============================\
             |            .-----.             |
             |           /  IN   \            |  <-- Central Badge (Covers background "0")
             |           \       /            |      Expands with gradient over 4s
             |            '-----'             |
              \===============================/
                             |  (Inhale Complete)
                             v
              /===============================\
             |          .-'     '-.           |  <-- Badge contracts back down (2–3s)
             |         /    OUT    \          |      Uncovers incremented "1" background
             |        |     (1)     |         |      Gradient resets
             |         \           /          |
              \===============================/
                             |  (Limit Reached)
                             v
                [ SCREEN 4: EXIT DECISION ]
              /===============================\
             |      Great session today!      |
             |         [ HOME PAGE ]          |  <-- Stacked Option 1
             |       [ RESTART EXERCISE ]     |  <-- Stacked Option 2
              \===============================/
Layout SpecificationScreen 1: Welcome ScreenCenter Main Button: A large circular button containing the bold text BEGIN.  Sub-Button (Cycles Counter): Positioned directly beneath the main button is a smaller circular frame containing a single integer (value range: $1 \le N \le 40$) representing planned breathing cycles.  Dynamic Focus Effect ("Teeter-Totter"):When BEGIN is focused: The BEGIN button expands slightly in scale while the Cycles button shrinks to its baseline size.When Cycles is focused: The Cycles button grows in scale while the BEGIN button shrinks.When Cycles is selected/clicked, rotating the dial increments or decrements the cycle total.Screen 2: Instruction ScreenTransition: Buttons from Screen 1 soft fade out completely.Text Structure (Stacked vertically):Usage Frequency: "It's your [N]th time today!"Encouraging Context: "Take a deep breath and settle in."  Instructional Subtext: "Follow the onscreen pulse. Press the encoder dial at any time to cancel."  Action Prompt: "Press encoder to begin..."Screen 3: Active Breathing LoopBackground Counter: A large, semi-transparent number (opacity: 30%) sitting dead-center in the background (starts at 0).Dynamic Badge: A fully opaque circular badge centered over the background counter.Inhale Phase (4.0s - Configurable via Web App): The badge expands outward to cover the entire background counter with text set to "IN". The background gradient smoothly shifts color.  Exhale Phase (2.0s – 3.0s): Text switches to "OUT". The badge contracts back down, revealing the background counter which increments by 1.Screen 4: Exit DecisionSummary Text: "Session Complete!"Menu Options: Two stacked rounded buttons: [Home Page] and [Restart Exercise].LVGL v8.2.0 Implementation CodeC#include "lvgl/lvgl.h"

static lv_obj_t *btn_begin;
static lv_obj_t *btn_cycles;
static lv_obj_t *badge;
static lv_obj_t *lbl_counter;
static uint8_t cycle_count = 3;
static uint8_t current_cycle = 0;

// Dynamic teeter-totter focus scaling callback
static void focus_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_FOCUSED) {
        lv_obj_set_style_transform_width(obj, 15, LV_PART_MAIN);
        lv_obj_set_style_transform_height(obj, 15, LV_PART_MAIN);
    } else if(code == LV_EVENT_DEFOCUSED) {
        lv_obj_set_style_transform_width(obj, 0, LV_PART_MAIN);
        lv_obj_set_style_transform_height(obj, 0, LV_PART_MAIN);
    }
}

void create_breathing_welcome_screen(lv_obj_t * parent) {
    // Parent Screen Setup
    lv_obj_t *scr = lv_obj_create(NULL);
    
    // Main "BEGIN" Circular Button
    btn_begin = lv_btn_create(scr);
    lv_obj_set_size(btn_begin, 140, 140);
    lv_obj_set_style_radius(btn_begin, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_align(btn_begin, LV_ALIGN_CENTER, 0, -25);
    lv_obj_add_event_cb(btn_begin, focus_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *lbl_begin = lv_label_create(btn_begin);
    lv_label_set_text(lbl_begin, "BEGIN");
    lv_obj_align(lbl_begin, LV_ALIGN_CENTER, 0, 0);

    // Secondary "Cycles" Circular Button
    btn_cycles = lv_btn_create(scr);
    lv_obj_set_size(btn_cycles, 60, 60);
    lv_obj_set_style_radius(btn_cycles, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_align(btn_cycles, LV_ALIGN_CENTER, 0, 85);
    lv_obj_add_event_cb(btn_cycles, focus_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *lbl_cycles = lv_label_create(btn_cycles);
    lv_label_set_text_fmt(lbl_cycles, "%d", cycle_count);
    lv_obj_align(lbl_cycles, LV_ALIGN_CENTER, 0, 0);

    // Register with Encoder Input Group
    lv_group_t * g = lv_group_create();
    lv_group_add_obj(g, btn_begin);
    lv_group_add_obj(g, btn_cycles);
    
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 400, 0, false);
}
2. Module: Radial To-Do List                 [ LIST AT REST ]
              /======================\
             |           /\          |  <-- Up Arrow (Visible if items above)
             |  O  First task...  (R)|
             | (O) Active Task... (O)|  <-- Magnified, Multi-line, Centered
             |     Extended detail   |
             |  O  Third task...  (G)|
             |           \/          |  <-- Down Arrow (Visible if items below)
              \======================/
                         |  (Check active item)
                         v
                [ INTERMEDIATE HOLD ]
              /======================\
             |           /\          |
             |  O  First task...  (R)|
             | (X) Strikethrough  (O)|  <-- Strikethrough applied instantly
             |     Muted color       |      Held frozen for 500ms
             |  O  Third task...  (G)|
             |           \/          |
              \======================/
                         |  (After 500ms)
                         v
                [ SHUFFLE DOWN ]
              /======================\
             |           /\          |
             |  O  First task...  (R)|  <-- Non-completed moves up
             |  O  Third task...  (G)|
             | (X) Strikethrough  (O)|  <-- Completed item now at bottom
             |           \/          |
              \======================/
Layout SpecificationVertical Flex Container: No native vertical scrollbar is drawn.  Row Composition:Left: Circular checkbox (lv_checkbox_t).Center: Task text label (LV_LABEL_LONG_DOT).Right: Circular priority dot indicator (Red, Orange, or Green as defined via Svelte Web App).  Active Element Magnification: The currently focused item anchors at $Y = 240$ (screen center). Its font size scales up, and text wraps up to 2 lines. Non-focused items scale down in font size and drop to 40% opacity.  Dynamic Arrow Indicators:Up Arrow ($\wedge$): Positioned at LV_ALIGN_TOP_MID. Automatically hides when scrolled to the top item.Down Arrow ($\vee$): Positioned at LV_ALIGN_BOTTOM_MID. Automatically hides when scrolled to the bottom item.Completion Workflow:Instant interaction: Clicking the encoder toggles the checkbox state, instantly applies a strikethrough decoration across the text, and shifts text color to dark gray.  Hold delay: The row remains locked in its current vertical list position for 500ms.  Reordering shuffle: After 500ms, the completed item smoothly moves down to the bottom index of the list container.  LVGL v8.2.0 Implementation CodeC#include "lvgl/lvgl.h"

typedef struct {
    lv_obj_t * row;
    lv_obj_t * container;
} task_data_t;

// Timer callback to move item after 500ms hold
static void shuffle_timer_cb(lv_timer_t * timer) {
    task_data_t * data = (task_data_t *)timer->user_data;
    
    // Shuffle object to the absolute bottom of the container index
    lv_obj_move_background(data->row); 
    
    // Re-align remaining items smoothly
    lv_obj_scroll_to_view(lv_obj_get_child(data->container, 0), LV_ANIM_ON);
    
    free(data);
    lv_timer_del(timer);
}

static void task_checkbox_cb(lv_event_t * e) {
    lv_obj_t * cb = lv_event_get_target(e);
    lv_obj_t * row = lv_obj_get_parent(cb);
    lv_obj_t * label = lv_obj_get_child(row, 1);

    if(lv_checkbox_is_checked(cb)) {
        // Apply instant visual strikethrough & dark gray tint
        lv_obj_set_style_text_decor(label, LV_TEXT_DECOR_STRIKE_THROUGH, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_hex(0x555555), LV_PART_MAIN);

        // Prepare 500ms hold timer
        task_data_t * data = malloc(sizeof(task_data_t));
        data->row = row;
        data->container = lv_obj_get_parent(row);

        lv_timer_create(shuffle_timer_cb, 500, data);
    }
}
3. Module: Sleep Tracker                        [ SCREEN 1: INTRO MAIN ]
                     /============================\
                    |       WELCOME BACK!         |
                    |        .---------.          |
                    |       /   START   \         |  <-- Main Circular Button
                    |       \  SESSION  /         |
                    |        '---------'          |
                    |          .------.           |
                    |         ( REVIEW )          |  <-- Secondary Circular Button
                    |          '------'           |
                     \============================/
                     /                            \
      (Click Start Session)                    (Click Weekly Review)
               v                                         v
   [ SCREEN A1: SET BEDTIME ]                 [ SCREEN B1: WEEKLY REVIEW ]
  /==========================\               /============================\
 |  <Personalized Message>   |              |    Your weekly average is   |
 |        .------.           |              |            8 hrs            | <-- Avg Hours
 |       / 19:59  \          |              |   <Encouraging message>     |
 |       \        /          |              |   .---------------------.   |
 |        '------'           |              |   | |  |  |  |  |  |  | |   | <-- 7-Day Bar Graph
 |     [START SESSION]       |              |   '---------------------'   |
 |        [ BACK ]           |              |        [ BACK BUTTON ]      | <-- Bottom Anchor
  \==========================/               \============================/
               |  (Click Start Session)
               v
   [ SCREEN A2: ACTIVE SLEEP ]
  /==========================\
 |     [ AWAKE? BUTTON ]     |  <-- Top Anchor Button
 |        .--------.         |
 |       /  Softly  \        |
 |      |  Pulsing   |       |  <-- Displays "Goodnight!"
 |       \  Circle  /        |
 |        '--------'         |
 |          19:59            |  <-- Time Display Below
  \==========================/
               |  (Click Awake?)
               v
   [ SCREEN A3: CONFIRMATION ]
  /==========================\
 |   [ ARE YOU SURE? (RED) ] |  <-- Warning Red Highlight
 |        [Nevermind]        |  <-- Smaller sub-button directly below
 |        .--------.         |
 |       / Pulsing  \        |
 |      |   Circle   |       |
 |       \          /        |
 |        '--------'         |
  \==========================/
     /                    \
(Click Are You Sure?)    (Click Nevermind)
      v                    v
  [ SCREEN A4: SUMMARY ]  (Returns to Screen A2)
 /==========================\
|  <Dynamic Sleep Summary>  |
|  <Calculated vs Goal>     |
|                           |
|  (Press any input to      |
|   return to Screen A1)    |
 \==========================/
Layout SpecificationScreen 1: Intro ScreenDesign Identity: Identical structure to Breathing App Screen 1.  Top Button: Large circular [START SESSION] button centered on upper display half.  Bottom Button: Smaller circular [WEEKLY REVIEW] button centered below.  Screen A1: Set Bedtime ScreenHeader: Dynamic greeting based on clock hour (e.g., "Good evening").  Time Display: Digital clock display inside a circular frame.  Buttons:[START SESSION] (Primary highlight): Advances to Screen A2.  [BACK] (Secondary button): Returns directly to Screen 1.Screen A2: Active Sleep ScreenTop Button: [AWAKE?] button anchored at the top rim of the display (LV_ALIGN_TOP_MID).Center Graphic: Softly pulsing circular badge with text "Goodnight!" rendered directly inside.  Bottom Clock: Current time display anchored at LV_ALIGN_BOTTOM_MID below the pulsing ring.  Screen A3: Confirmation CheckTrigger: Clicked [AWAKE?] on Screen A2.Top Button Change: The [AWAKE?] button updates its background color to warning red (0xE74C3C) and changes text to "ARE YOU SURE?".Sub-Button: A smaller button displaying "Nevermind" reveals itself directly beneath the warning button.Branching:Click [Nevermind]: Reverts UI state back to Screen A2.Click [ARE YOU SURE?]: Advances to Screen A4.Screen A4: Summary ScreenDisplay: Summary text fades in describing time spent sleeping and evaluation relative to targets.Dismissal: Screen stays active until any physical encoder input is registered, returning back to Screen A1.Screen B1: Weekly Review ScreenHeader: "Your weekly average is".  Primary Metric: Large numerical display showing weekly sleep average (e.g., 8 hrs).  Calculation Boundary Rule: Sleep duration calculation windows run between noon-to-noon periods (12:00 PM Day N to 12:00 PM Day N+1) to correctly group late-night sleep.  Subtext: Encouraging message based on average hours logged.  Chart Widget: 7-day bar graph showing nightly durations (lv_chart_t).  Back Navigation: Explicit [BACK] button anchored at LV_ALIGN_BOTTOM_MID returning directly to Screen 1.LVGL v8.2.0 Implementation CodeC#include "lvgl/lvgl.h"

static bool is_confirm_state = false;
static lv_obj_t * btn_awake;
static lv_obj_t * btn_nevermind;

// Screen B1: Weekly Review Implementation
void create_screen_B1_weekly_review(lv_obj_t * parent) {
    lv_obj_t * scr = lv_obj_create(NULL);

    // Header
    lv_obj_t * lbl_header = lv_label_create(scr);
    lv_label_set_text(lbl_header, "Your weekly average is");
    lv_obj_align(lbl_header, LV_ALIGN_TOP_MID, 0, 35);

    // Main Metric Display
    lv_obj_t * lbl_avg = lv_label_create(scr);
    lv_label_set_text(lbl_avg, "8 hrs");
    lv_obj_set_style_text_font(lbl_avg, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_obj_align(lbl_avg, LV_ALIGN_TOP_MID, 0, 65);

    // Subtext
    lv_obj_t * lbl_sub = lv_label_create(scr);
    lv_label_set_text(lbl_sub, "Great consistency this week!");
    lv_obj_align(lbl_sub, LV_ALIGN_TOP_MID, 0, 110);

    // 7-Day Bar Chart
    lv_obj_t * chart = lv_chart_create(scr);
    lv_obj_set_size(chart, 280, 120);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 30);
    lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(chart, 7);

    lv_chart_series_t * ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_next_value(chart, ser, 7);
    lv_chart_set_next_value(chart, ser, 8);
    lv_chart_set_next_value(chart, ser, 6);
    lv_chart_set_next_value(chart, ser, 9);
    lv_chart_set_next_value(chart, ser, 8);
    lv_chart_set_next_value(chart, ser, 7);
    lv_chart_set_next_value(chart, ser, 8);

    // Bottom Back Button
    lv_obj_t * btn_back = lv_btn_create(scr);
    lv_obj_set_size(btn_back, 110, 40);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "BACK");
    lv_obj_align(lbl_back, LV_ALIGN_CENTER, 0, 0);

    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 400, 0, false);
}

// Awake / Are You Sure Toggle Handler (Screen A2 -> Screen A3)
static void awake_click_cb(lv_event_t * e) {
    if (!is_confirm_state) {
        // Transition to Screen A3 State
        lv_obj_set_style_bg_color(btn_awake, lv_color_hex(0xE74C3C), LV_PART_MAIN);
        lv_label_set_text(lv_obj_get_child(btn_awake, 0), "ARE YOU SURE?");
        lv_obj_clear_flag(btn_nevermind, LV_OBJ_FLAG_HIDDEN);
        is_confirm_state = true;
    } else {
        // Advance to Screen A4 Summary
        // load_screen_A4();
    }
}

static void nevermind_click_cb(lv_event_t * e) {
    // Revert to Screen A2 State
    lv_obj_set_style_bg_color(btn_awake, lv_color_hex(0x2ECC71), LV_PART_MAIN);
    lv_label_set_text(lv_obj_get_child(btn_awake, 0), "AWAKE?");
    lv_obj_add_flag(btn_nevermind, LV_OBJ_FLAG_HIDDEN);
    is_confirm_state = false;
}