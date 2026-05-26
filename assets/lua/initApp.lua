-- initApp.lua
-- Called during Application::create(). All Lua API surfaces (SDL, UI, etc.)
-- are bound before this runs.

-- Fonts must be loaded before UI.LoadDocument().
UI.LoadFont("assets/rmlui/LatoLatin-Regular.ttf")
UI.LoadFont("assets/rmlui/LatoLatin-Bold.ttf")
UI.LoadFont("assets/rmlui/LatoLatin-Italic.ttf")

--SDL.LoadScene("YourScene")


