-- Run this script once to store this function in the lua vm.
-- Does not actually initiate the tick function, that is done through code or another script.

-- Ensure the UGE.Ticking namespace exists without clobbering UGE.
UGE.Ticking = UGE.Ticking or {}
Data.Set ("luaTests.stateStop", 0)

-- ── tickTest ─────────────────────────────────────────────────────────────────
function UGE.Ticking.tickTest()

    if Data.Get ("luaTests.stateStop") == 1 then --Clean up
        Data.Set ("luaTests.coinPosition.x", 600)
        Data.Set ("luaTests.stateStop", 0)
        UGE.RemoveTick(UGE.GetTickId("tickTest"))
        return
    end

    xPos = Data.Get ("luaTests.coinPosition.x")

    newXPos = xPos + (Data.Get("luaTests.coinDirection") * 2)

    if newXPos <= 0 or newXPos >= 1150 then
        Data.Set ("luaTests.stateStop", 1) --Stop the tick function on the next tick.
    end

    Data.Set ("luaTests.coinPosition.x", newXPos)
end


