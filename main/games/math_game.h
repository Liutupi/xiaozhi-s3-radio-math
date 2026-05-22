#ifndef MATH_GAME_H_
#define MATH_GAME_H_

#include "display.h"

#include <lvgl.h>

#include <cstdint>

class MathGame {
public:
    MathGame() = default;
    ~MathGame();

    void Start(Display* display);
    void Stop();
    bool HandleClick();
    bool HandleDoubleClick();
    bool MoveRight();
    bool MoveLeft();
    bool IsRunning() const { return running_; }

    struct Question {
        const char* topic;
        const char* prompt;
        const char* options[4];
        uint8_t answer;
        const char* hint;
    };

private:
    Display* display_ = nullptr;
    lv_obj_t* layer_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* progress_label_ = nullptr;
    lv_obj_t* progress_bar_ = nullptr;
    lv_obj_t* question_card_ = nullptr;
    lv_obj_t* prompt_label_ = nullptr;
    lv_obj_t* hint_label_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* combo_label_ = nullptr;
    lv_obj_t* footer_label_ = nullptr;
    lv_obj_t* feedback_panel_ = nullptr;
    lv_obj_t* feedback_label_ = nullptr;
    lv_obj_t* option_boxes_[4] = {};
    lv_obj_t* option_badges_[4] = {};
    lv_obj_t* option_labels_[4] = {};

    bool running_ = false;
    bool showing_feedback_ = false;
    bool finished_ = false;
    int width_ = 0;
    int height_ = 0;
    uint8_t question_index_ = 0;
    uint8_t selected_ = 0;
    uint8_t correct_count_ = 0;
    uint8_t combo_ = 0;
    uint8_t best_combo_ = 0;
    int score_ = 0;

    void CreateUi();
    void DestroyUi();
    void ResetGame();
    void DrawQuestion();
    void DrawOptions();
    void SubmitAnswer();
    void NextQuestion();
    void DrawFeedback(bool correct);
    void DrawSummary();
};

#endif  // MATH_GAME_H_
