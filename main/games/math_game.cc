#include "math_game.h"

#include <algorithm>

namespace {

constexpr int kOptionCount = 4;
constexpr uint32_t kBg = 0x07111f;
constexpr uint32_t kPanel = 0x101729;
constexpr uint32_t kCard = 0x16345f;
constexpr uint32_t kCardDeep = 0x0b1f3a;
constexpr uint32_t kAccent = 0xffd166;
constexpr uint32_t kAccentSoft = 0xfef3c7;
constexpr uint32_t kGood = 0x22c55e;
constexpr uint32_t kBad = 0xef4444;
constexpr uint32_t kText = 0xf8fafc;
constexpr uint32_t kMuted = 0x94a3b8;

const MathGame::Question kQuestions[] = {
    {"Mental Math", "125 + 75 = ?", {"190", "200", "210", "180"}, 1, "Make 100: 125 + 75 = 200."},
    {"Mental Math", "25 x 16 = ?", {"300", "350", "400", "450"}, 2, "25 x 4 x 4 = 400."},
    {"Smart Order", "58 + 39 + 42 = ?", {"129", "139", "149", "159"}, 1, "58 + 42 = 100."},
    {"Smart Order", "99 x 46 = ?", {"4554", "4544", "4654", "4564"}, 0, "100 x 46 - 46."},
    {"Decimals", "2.5 - 1.3 = ?", {"1.1", "1.2", "1.3", "1.4"}, 1, "Subtract tenths."},
    {"Decimals", "0.3 x 10 = ?", {"0.03", "0.3", "3", "30"}, 2, "Move the point right."},
    {"Triangles", "Triangle angles sum?", {"90", "120", "180", "360"}, 2, "Every triangle sums to 180."},
    {"Triangles", "Equilateral angle?", {"45", "60", "90", "120"}, 1, "180 / 3 = 60."},
    {"Logic", "10 heads, 28 legs. Rabbits?", {"3", "4", "5", "6"}, 1, "Extra legs: 8 / 2 = 4."},
    {"Logic", "12 vehicles, 32 wheels. Cars?", {"3", "4", "5", "6"}, 1, "Cars add 2 wheels each."},
    {"Challenge", "99 x 99 + 99 = ?", {"9801", "9900", "9999", "10000"}, 1, "99 x (99 + 1)."},
    {"Challenge", "3.14 + 2.86 = ?", {"5.90", "6.00", "6.10", "6.20"}, 1, "Pairs neatly to 6."},
};

constexpr uint8_t kQuestionCount = sizeof(kQuestions) / sizeof(kQuestions[0]);

void StyleBox(lv_obj_t* obj, uint32_t bg, uint32_t border, int border_width, int radius) {
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(obj, border_width, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

void StyleLabel(lv_obj_t* label, uint32_t color, lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_obj_set_style_text_line_space(label, 0, 0);
}

lv_obj_t* AddLabel(lv_obj_t* parent, int x, int y, int w, uint32_t color, lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    StyleLabel(label, color, align);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}

}  // namespace

MathGame::~MathGame() {
    Stop();
}

void MathGame::Start(Display* display) {
    if (running_ || display == nullptr) {
        return;
    }

    display_ = display;
    width_ = display_->width();
    height_ = display_->height();

    {
        DisplayLockGuard lock(display_);
        CreateUi();
        ResetGame();
    }

    running_ = true;
}

void MathGame::Stop() {
    if (display_ != nullptr) {
        DisplayLockGuard lock(display_);
        DestroyUi();
    }
    running_ = false;
    display_ = nullptr;
}

bool MathGame::HandleClick() {
    if (!running_ || display_ == nullptr) {
        return false;
    }

    DisplayLockGuard lock(display_);
    if (finished_) {
        ResetGame();
    } else if (showing_feedback_) {
        NextQuestion();
    } else {
        SubmitAnswer();
    }
    return true;
}

bool MathGame::HandleDoubleClick() {
    return MoveLeft();
}

bool MathGame::MoveRight() {
    if (!running_ || display_ == nullptr) {
        return false;
    }

    DisplayLockGuard lock(display_);
    if (!showing_feedback_ && !finished_) {
        selected_ = (selected_ + 1) % kOptionCount;
        DrawOptions();
    }
    return true;
}

bool MathGame::MoveLeft() {
    if (!running_ || display_ == nullptr) {
        return false;
    }

    DisplayLockGuard lock(display_);
    if (!showing_feedback_ && !finished_) {
        selected_ = (selected_ + kOptionCount - 1) % kOptionCount;
        DrawOptions();
    }
    return true;
}

void MathGame::CreateUi() {
    layer_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(layer_, width_, height_);
    lv_obj_set_pos(layer_, 0, 0);
    StyleBox(layer_, kBg, kBg, 0, 0);

    lv_obj_t* header = lv_obj_create(layer_);
    lv_obj_set_size(header, width_, 20);
    lv_obj_set_pos(header, 0, 0);
    StyleBox(header, kPanel, kPanel, 0, 0);

    title_label_ = AddLabel(layer_, 6, 3, width_ - 48, kAccent);
    progress_label_ = AddLabel(layer_, width_ - 45, 3, 40, kMuted, LV_TEXT_ALIGN_RIGHT);

    score_label_ = AddLabel(layer_, 7, 23, 56, kText, LV_TEXT_ALIGN_CENTER);
    combo_label_ = AddLabel(layer_, 65, 23, 56, kText, LV_TEXT_ALIGN_CENTER);

    lv_obj_t* score_pill = lv_obj_create(layer_);
    lv_obj_set_size(score_pill, 56, 17);
    lv_obj_set_pos(score_pill, 7, 22);
    StyleBox(score_pill, 0x0f2f48, 0x1e5f7a, 1, 4);
    lv_obj_move_background(score_pill);

    lv_obj_t* combo_pill = lv_obj_create(layer_);
    lv_obj_set_size(combo_pill, 56, 17);
    lv_obj_set_pos(combo_pill, 65, 22);
    StyleBox(combo_pill, 0x2a1f0b, 0x6b4e16, 1, 4);
    lv_obj_move_background(combo_pill);

    progress_bar_ = lv_obj_create(layer_);
    lv_obj_set_size(progress_bar_, 112, 3);
    lv_obj_set_pos(progress_bar_, 8, 42);
    StyleBox(progress_bar_, kAccent, kAccent, 0, 1);

    question_card_ = lv_obj_create(layer_);
    lv_obj_set_size(question_card_, width_ - 14, 40);
    lv_obj_set_pos(question_card_, 7, 49);
    StyleBox(question_card_, kCardDeep, 0x235a7c, 1, 6);
    prompt_label_ = AddLabel(question_card_, 5, 6, width_ - 24, kText, LV_TEXT_ALIGN_CENTER);

    for (int i = 0; i < kOptionCount; ++i) {
        const int col = i % 2;
        const int row = i / 2;
        option_boxes_[i] = lv_obj_create(layer_);
        lv_obj_set_size(option_boxes_[i], 55, 27);
        lv_obj_set_pos(option_boxes_[i], 7 + col * 59, 94 + row * 31);
        StyleBox(option_boxes_[i], kCard, 0x235a7c, 1, 5);

        option_badges_[i] = lv_obj_create(option_boxes_[i]);
        lv_obj_set_size(option_badges_[i], 15, 15);
        lv_obj_set_pos(option_badges_[i], 4, 6);
        StyleBox(option_badges_[i], 0x0b1220, 0x0b1220, 0, 7);

        lv_obj_t* badge_label = AddLabel(option_badges_[i], 0, 0, 15, kAccent, LV_TEXT_ALIGN_CENTER);
        lv_label_set_text_fmt(badge_label, "%c", 'A' + i);

        option_labels_[i] = AddLabel(option_boxes_[i], 21, 6, 30, kText, LV_TEXT_ALIGN_CENTER);
        lv_label_set_long_mode(option_labels_[i], LV_LABEL_LONG_DOT);
    }

    hint_label_ = AddLabel(layer_, 6, height_ - 31, width_ - 12, kMuted, LV_TEXT_ALIGN_CENTER);
    footer_label_ = AddLabel(layer_, 6, height_ - 15, width_ - 12, kAccent, LV_TEXT_ALIGN_CENTER);

    feedback_panel_ = lv_obj_create(layer_);
    lv_obj_set_size(feedback_panel_, width_ - 16, 82);
    lv_obj_set_pos(feedback_panel_, 8, 39);
    StyleBox(feedback_panel_, kPanel, kAccent, 2, 6);
    feedback_label_ = AddLabel(feedback_panel_, 5, 9, width_ - 26, kText, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(feedback_panel_, LV_OBJ_FLAG_HIDDEN);
}

void MathGame::DestroyUi() {
    if (layer_ != nullptr) {
        lv_obj_del(layer_);
    }
    layer_ = nullptr;
    title_label_ = nullptr;
    progress_label_ = nullptr;
    progress_bar_ = nullptr;
    question_card_ = nullptr;
    prompt_label_ = nullptr;
    hint_label_ = nullptr;
    score_label_ = nullptr;
    combo_label_ = nullptr;
    footer_label_ = nullptr;
    feedback_panel_ = nullptr;
    feedback_label_ = nullptr;
    for (auto& box : option_boxes_) {
        box = nullptr;
    }
    for (auto& badge : option_badges_) {
        badge = nullptr;
    }
    for (auto& label : option_labels_) {
        label = nullptr;
    }
}

void MathGame::ResetGame() {
    question_index_ = 0;
    selected_ = 0;
    correct_count_ = 0;
    combo_ = 0;
    best_combo_ = 0;
    score_ = 0;
    showing_feedback_ = false;
    finished_ = false;
    DrawQuestion();
}

void MathGame::DrawQuestion() {
    const auto& q = kQuestions[question_index_];
    showing_feedback_ = false;
    lv_obj_add_flag(feedback_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(title_label_, q.topic);
    lv_label_set_text_fmt(progress_label_, "%u/%u", question_index_ + 1, kQuestionCount);
    lv_label_set_text_fmt(score_label_, "SCORE %d", score_);
    lv_label_set_text_fmt(combo_label_, "COMBO %u", combo_);
    lv_label_set_text(prompt_label_, q.prompt);
    lv_obj_set_width(progress_bar_, ((question_index_ + 1) * 112) / kQuestionCount);
    lv_label_set_text(hint_label_, "GPIO39 choose");
    lv_label_set_text(footer_label_, "BOOT answer | hold menu");
    DrawOptions();
}

void MathGame::DrawOptions() {
    const auto& q = kQuestions[question_index_];
    for (int i = 0; i < kOptionCount; ++i) {
        const bool active = i == selected_;
        StyleBox(option_boxes_[i], active ? kAccentSoft : kCard, active ? kAccent : 0x235a7c, active ? 2 : 1, 5);
        StyleBox(option_badges_[i], active ? kAccent : 0x0b1220, active ? kAccent : 0x0b1220, 0, 7);
        lv_obj_set_style_text_color(option_labels_[i], lv_color_hex(active ? 0x111827 : kText), 0);
        lv_label_set_text(option_labels_[i], q.options[i]);
    }
}

void MathGame::SubmitAnswer() {
    const auto& q = kQuestions[question_index_];
    const bool correct = selected_ == q.answer;
    if (correct) {
        ++correct_count_;
        ++combo_;
        best_combo_ = std::max(best_combo_, combo_);
        score_ += 10 + std::min<uint8_t>(combo_, 5) * 2;
    } else {
        combo_ = 0;
    }
    DrawFeedback(correct);
}

void MathGame::NextQuestion() {
    if (question_index_ + 1 >= kQuestionCount) {
        DrawSummary();
        return;
    }
    ++question_index_;
    selected_ = 0;
    DrawQuestion();
}

void MathGame::DrawFeedback(bool correct) {
    const auto& q = kQuestions[question_index_];
    showing_feedback_ = true;
    StyleBox(feedback_panel_, correct ? 0x064e3b : 0x7f1d1d, correct ? kGood : kBad, 2, 6);
    lv_label_set_text_fmt(feedback_label_, "%s\n%c. %s\n%s\nBOOT next",
                          correct ? "RIGHT!" : "NOT YET",
                          'A' + q.answer,
                          q.options[q.answer],
                          q.hint);
    lv_obj_clear_flag(feedback_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(score_label_, "SCORE %d", score_);
    lv_label_set_text_fmt(combo_label_, "COMBO %u", combo_);
    lv_label_set_text(hint_label_, correct ? "Nice thinking." : "Review the hint.");
    lv_label_set_text(footer_label_, "BOOT next | hold menu");
}

void MathGame::DrawSummary() {
    finished_ = true;
    showing_feedback_ = false;
    lv_obj_add_flag(feedback_panel_, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(title_label_, "Math Quest");
    lv_label_set_text(progress_label_, "DONE");
    lv_label_set_text(prompt_label_, "Challenge Complete");
    lv_label_set_text_fmt(score_label_, "SCORE %d", score_);
    lv_label_set_text_fmt(combo_label_, "BEST %u", best_combo_);
    lv_obj_set_width(progress_bar_, 112);

    for (int i = 0; i < kOptionCount; ++i) {
        StyleBox(option_boxes_[i], kPanel, 0x334155, 1, 5);
        StyleBox(option_badges_[i], 0x0b1220, 0x0b1220, 0, 7);
        lv_obj_set_style_text_color(option_labels_[i], lv_color_hex(kText), 0);
    }
    lv_label_set_text_fmt(option_labels_[0], "%u/%u", correct_count_, kQuestionCount);
    lv_label_set_text_fmt(option_labels_[1], "%u%%", static_cast<unsigned>((correct_count_ * 100) / kQuestionCount));
    lv_label_set_text_fmt(option_labels_[2], "%d pts", score_);
    lv_label_set_text(option_labels_[3], correct_count_ >= 10 ? "STAR" : "TRY");
    lv_label_set_text(hint_label_, "Great work.");
    lv_label_set_text(footer_label_, "BOOT restart | hold menu");
}
