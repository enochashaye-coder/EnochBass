#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// v3.4: a dropdown that also steps through its items. The small up / down arrows on its right edge (top half = previous,
// bottom half = next), the Up and Down keys and the mouse wheel each move the selection by one item and apply it at once.
// The wheel is used up by the dropdown, so wheeling over it does not also scroll the panel behind it.
class StepCombo : public juce::ComboBox
{
public:
    static constexpr int arrowW = 16;

    void step (int dir)
    {
        const int n = getNumItems();
        if (n <= 0) return;
        const int cur = getSelectedItemIndex();
        const int next = juce::jlimit (0, n - 1, (cur < 0 ? 0 : cur) + dir);
        if (next != cur) setSelectedItemIndex (next, juce::sendNotificationSync);
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const float d = w.deltaY != 0.f ? w.deltaY : w.deltaX;
        wheelAcc += d;
        if (wheelAcc >= 0.5f)       { wheelAcc = 0.f; step (-1); }   // wheel up = previous item
        else if (wheelAcc <= -0.5f) { wheelAcc = 0.f; step (1); }    // wheel down = next item
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::upKey)   { step (-1); return true; }
        if (k == juce::KeyPress::downKey) { step (1);  return true; }
        return juce::ComboBox::keyPressed (k);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x >= getWidth() - arrowW && e.mods.isLeftButtonDown())
        {
            step (e.y < getHeight() / 2 ? -1 : 1);
            return;
        }
        juce::ComboBox::mouseDown (e);
    }

    void paint (juce::Graphics& g) override
    {
        juce::ComboBox::paint (g);
        const auto area = getLocalBounds().removeFromRight (arrowW);
        g.setColour (findColour (juce::ComboBox::backgroundColourId));
        g.fillRect (area);
        const auto r = area.toFloat().reduced (4.f, 0.f);
        const float cx = r.getCentreX();
        const float yUp = r.getY() + r.getHeight() * 0.30f, yDn = r.getY() + r.getHeight() * 0.70f;
        juce::Path up, dn;
        up.addTriangle (cx - 3.5f, yUp + 3.f, cx + 3.5f, yUp + 3.f, cx, yUp - 0.5f);
        dn.addTriangle (cx - 3.5f, yDn - 3.f, cx + 3.5f, yDn - 3.f, cx, yDn + 0.5f);
        g.setColour (findColour (juce::ComboBox::arrowColourId));
        g.fillPath (up); g.fillPath (dn);
    }

private:
    float wheelAcc = 0.f;
};
