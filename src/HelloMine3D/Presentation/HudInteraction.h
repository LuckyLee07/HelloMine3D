#pragma once

// Ephemeral UI ownership; never stored in a world or settings file.
class HudInteraction
{
  public:
    enum class Page { Game, Pointer, Journal, Map };
    Page page() const noexcept { return m_page; }
    bool ownsInput() const noexcept { return m_page != Page::Game; }
    bool togglePointer(bool editing = false) noexcept
    {
        if (editing) return false;
        m_page = ownsInput() ? Page::Game : Page::Pointer;
        return true;
    }
    bool open(Page page) noexcept
    {
        if (!ownsInput() || (page != Page::Journal && page != Page::Map))
            return false;
        m_page = page;
        return true;
    }
    bool dismiss() noexcept
    {
        const bool handled = ownsInput();
        m_page = Page::Game;
        return handled;
    }
  private:
    Page m_page = Page::Game;
};
