#pragma once

#include "View.h"
#include <Font.h>

class TextView : public View {
public:
    const char* text = nullptr;
    const Font* font = nullptr;

    void draw(Gui& gui) override;
};
