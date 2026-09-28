#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>

#include <stdio.h>
#include <string>

#include "game_core.hpp"


PSP_MODULE_INFO(
    "DeersMahjong",
    PSP_MODULE_USER,
    1,
    0
);

PSP_MAIN_THREAD_ATTR(
    THREAD_ATTR_USER |
    THREAD_ATTR_VFPU
);


// =====================================================
// EXIT CALLBACK
// =====================================================

int exitCallback(
    int arg1,
    int arg2,
    void* common
)
{
    sceKernelExitGame();

    return 0;
}


int callbackThread(
    SceSize args,
    void* argp
)
{
    int cbid =
        sceKernelCreateCallback(
            "Exit Callback",
            exitCallback,
            NULL
        );


    sceKernelRegisterExitCallback(
        cbid
    );


    sceKernelSleepThreadCB();


    return 0;
}


int setupCallbacks()
{
    int threadId =
        sceKernelCreateThread(
            "callback_thread",
            callbackThread,
            0x11,
            0xFA0,
            0,
            0
        );


    if (threadId >= 0)
    {
        sceKernelStartThread(
            threadId,
            0,
            0
        );
    }


    return threadId;
}


// =====================================================
// DISPLAY HELPERS
// =====================================================

void setColor(
    unsigned int color
)
{
    pspDebugScreenSetTextColor(
        color
    );
}


void printTileRow(
    const Player& human,
    int selected
)
{
    for (int i = 0;
         i <
            static_cast<int>(
                human.hand.size()
            );
         ++i)
    {
        if (i == selected)
        {
            setColor(
                0xFF00FFFF
            );

            pspDebugScreenPrintf(
                "[%s]",
                human.hand[i].c_str()
            );
        }

        else
        {
            setColor(
                0xFFFFFFFF
            );

            pspDebugScreenPrintf(
                " %s ",
                human.hand[i].c_str()
            );
        }
    }


    pspDebugScreenPrintf(
        "\n"
    );
}


void printMelds(
    const Player& player
)
{
    if (player.melds.empty())
        return;


    setColor(
        0xFF80FFFF
    );


    pspDebugScreenPrintf(
        "FORMED: "
    );


    for (size_t i = 0;
         i < player.melds.size();
         ++i)
    {
        pspDebugScreenPrintf(
            "%s[",
            player.melds[i].
                kind.c_str()
        );


        for (size_t x = 0;
             x <
                player.melds[i].
                    logical.size();
             ++x)
        {
            pspDebugScreenPrintf(
                "%s",
                player.melds[i].
                    logical[x].
                    c_str()
            );


            if (
                x + 1 <
                player.melds[i].
                    logical.size()
            )
            {
                pspDebugScreenPrintf(
                    " "
                );
            }
        }


        pspDebugScreenPrintf(
            "] "
        );
    }


    pspDebugScreenPrintf(
        "\n"
    );
}


void render(
    const GameCore& game,
    int selectedTile,
    bool musicEnabled
)
{
    pspDebugScreenClear();


    setColor(
        0xFFFFFFFF
    );


    pspDebugScreenPrintf(
        "THE DEER'S MAHJONG TABLE OF MISFORTUNE\n"
    );


    setColor(
        0xFF80FF80
    );


    pspDebugScreenPrintf(
        "PSP HOMEBREW BUILD 0.1\n"
    );


    setColor(
        0xFFFFFFFF
    );


    pspDebugScreenPrintf(
        "Wall: %d     Music: %s\n",
        game.wallCount(),
        musicEnabled
            ? "ON"
            : "OFF"
    );


    pspDebugScreenPrintf(
        "-----------------------------------------------\n"
    );


    // =================================================
    // BOT STATUS
    // =================================================

    for (int i = 1;
         i < 4;
         ++i)
    {
        const Player& bot =
            game.player(i);


        pspDebugScreenPrintf(
            "%s : %d concealed | %d formed\n",
            bot.name.c_str(),
            static_cast<int>(
                bot.hand.size()
            ),
            static_cast<int>(
                bot.melds.size()
            )
        );
    }


    pspDebugScreenPrintf(
        "\n"
    );


    // =================================================
    // RIVER
    // =================================================

    setColor(
        0xFFAAAAAA
    );


    pspDebugScreenPrintf(
        "RIVER: "
    );


    const std::vector<DiscardEntry>& river =
        game.river();


    int begin =
        static_cast<int>(
            river.size()
        ) - 12;


    if (begin < 0)
        begin = 0;


    for (int i = begin;
         i <
            static_cast<int>(
                river.size()
            );
         ++i)
    {
        pspDebugScreenPrintf(
            "%s ",
            river[i].tile.c_str()
        );
    }


    pspDebugScreenPrintf(
        "\n\n"
    );


    // =================================================
    // HUMAN HAND
    // =================================================

    const Player& human =
        game.player(0);


    setColor(
        0xFFFFFFFF
    );


    pspDebugScreenPrintf(
        "YOUR HAND (%d)\n",
        static_cast<int>(
            human.hand.size()
        )
    );


    printTileRow(
        human,
        selectedTile
    );


    printMelds(
        human
    );


    pspDebugScreenPrintf(
        "\n"
    );


    // =================================================
    // CLAIM WINDOW
    // =================================================

    const ClaimPrompt& prompt =
        game.claimPrompt();


    if (prompt.active)
    {
        setColor(
            0xFF00FFFF
        );


        pspDebugScreenPrintf(
            "CLAIM: %s discarded %s\n",
            game.player(
                static_cast<int>(
                    prompt.from
                )
            ).name.c_str(),
            prompt.tile.c_str()
        );


        setColor(
            0xFFFFFFFF
        );


        pspDebugScreenPrintf(
            "O = DRAW/PASS"
        );


        if (prompt.mahjong)
        {
            pspDebugScreenPrintf(
                "   X = MAHJONG"
            );
        }


        pspDebugScreenPrintf(
            "\n"
        );


        if (!prompt.chows.empty())
        {
            int index =
                prompt.selectedChow;


            pspDebugScreenPrintf(
                "SQUARE = CHOW ["
            );


            for (size_t i = 0;
                 i <
                    prompt.chows[index].
                        size();
                 ++i)
            {
                pspDebugScreenPrintf(
                    "%s",
                    prompt.chows[index][i].
                        c_str()
                );


                if (
                    i + 1 <
                    prompt.chows[index].
                        size()
                )
                {
                    pspDebugScreenPrintf(
                        " "
                    );
                }
            }


            pspDebugScreenPrintf(
                "]   R = next CHOW\n"
            );
        }


        if (prompt.pung)
        {
            pspDebugScreenPrintf(
                "TRIANGLE = PUNG\n"
            );
        }


        if (prompt.kong)
        {
            pspDebugScreenPrintf(
                "L = KONG\n"
            );
        }
    }

    else if (
        game.humanNeedsDiscard() &&
        !game.handOver()
    )
    {
        setColor(
            0xFFFFFFFF
        );


        pspDebugScreenPrintf(
            "LEFT/RIGHT = select tile\n"
        );


        pspDebugScreenPrintf(
            "X = DISCARD"
            "   TRIANGLE = SELF KONG\n"
        );
    }


    // =================================================
    // RESULT
    // =================================================

    if (game.handOver())
    {
        pspDebugScreenPrintf(
            "\n"
        );


        if (game.hasWinner())
        {
            setColor(
                0xFF00FFFF
            );


            pspDebugScreenPrintf(
                "************* MAHJONG *************\n"
            );


            pspDebugScreenPrintf(
                "WINNER: %s\n",
                game.player(
                    static_cast<int>(
                        game.winner()
                    )
                ).name.c_str()
            );


            pspDebugScreenPrintf(
                "METHOD: %s\n",
                game.winningMethod().
                    c_str()
            );
        }

        else
        {
            setColor(
                0xFFAAAAAA
            );


            pspDebugScreenPrintf(
                "WALL EMPTY - DRAWN HAND\n"
            );
        }


        setColor(
            0xFFFFFFFF
        );


        pspDebugScreenPrintf(
            "START = NEW HAND\n"
        );
    }


    // =================================================
    // MESSAGE
    // =================================================

    pspDebugScreenPrintf(
        "\n"
    );


    setColor(
        0xFF80FF80
    );


    pspDebugScreenPrintf(
        "%s\n",
        game.message().c_str()
    );


    setColor(
        0xFFAAAAAA
    );


    pspDebugScreenPrintf(
        "\nSTART = new hand   SELECT = music toggle\n"
    );
}


// =====================================================
// MAIN
// =====================================================

int main()
{
    setupCallbacks();


    pspDebugScreenInit();


    sceCtrlSetSamplingCycle(
        0
    );


    sceCtrlSetSamplingMode(
        PSP_CTRL_MODE_ANALOG
    );


    GameCore game;


    uint32_t seed =
        static_cast<uint32_t>(
            sceKernelGetSystemTimeLow()
        );


    game.newHand(
        seed
    );


    int selectedTile =
        0;


    bool musicEnabled =
        true;


    unsigned int previousButtons =
        0;


    while (1)
    {
        // Let bots / automatic game flow progress.
        game.advance();


        const Player& human =
            game.player(0);


        if (
            human.hand.empty()
        )
        {
            selectedTile =
                0;
        }

        else if (
            selectedTile >=
                static_cast<int>(
                    human.hand.size()
                )
        )
        {
            selectedTile =
                static_cast<int>(
                    human.hand.size()
                ) - 1;
        }


        render(
            game,
            selectedTile,
            musicEnabled
        );


        SceCtrlData pad;


        sceCtrlPeekBufferPositive(
            &pad,
            1
        );


        unsigned int pressed =
            pad.Buttons &
            ~previousButtons;


        previousButtons =
            pad.Buttons;


        // =================================================
        // GLOBAL
        // =================================================

        if (
            pressed &
            PSP_CTRL_START
        )
        {
            seed =
                static_cast<uint32_t>(
                    sceKernelGetSystemTimeLow()
                );


            game.newHand(
                seed
            );


            selectedTile =
                0;
        }


        if (
            pressed &
            PSP_CTRL_SELECT
        )
        {
            musicEnabled =
                !musicEnabled;
        }


        // =================================================
        // CLAIM WINDOW
        // =================================================

        if (
            game.claimPrompt().
                active &&
            !game.handOver()
        )
        {
            if (
                pressed &
                PSP_CTRL_CIRCLE
            )
            {
                game.humanPass();
            }


            if (
                pressed &
                PSP_CTRL_CROSS
            )
            {
                game.humanClaimMahjong();
            }


            if (
                pressed &
                PSP_CTRL_SQUARE
            )
            {
                game.humanClaimChow();
            }


            if (
                pressed &
                PSP_CTRL_TRIANGLE
            )
            {
                game.humanClaimPung();
            }


            if (
                pressed &
                PSP_CTRL_LTRIGGER
            )
            {
                game.humanClaimKong();
            }


            if (
                pressed &
                PSP_CTRL_RTRIGGER
            )
            {
                game.selectNextChow(
                    1
                );
            }
        }


        // =================================================
        // HUMAN NORMAL TURN
        // =================================================

        else if (
            game.humanNeedsDiscard() &&
            !game.handOver()
        )
        {
            int count =
                static_cast<int>(
                    human.hand.size()
                );


            if (
                count > 0 &&
                (
                    pressed &
                    PSP_CTRL_LEFT
                )
            )
            {
                --selectedTile;


                if (
                    selectedTile < 0
                )
                {
                    selectedTile =
                        count - 1;
                }
            }


            if (
                count > 0 &&
                (
                    pressed &
                    PSP_CTRL_RIGHT
                )
            )
            {
                ++selectedTile;


                if (
                    selectedTile >= count
                )
                {
                    selectedTile =
                        0;
                }
            }


            // X = discard.
            if (
                pressed &
                PSP_CTRL_CROSS
            )
            {
                if (
                    game.humanDiscard(
                        selectedTile
                    )
                )
                {
                    if (
                        selectedTile >=
                            static_cast<int>(
                                game.player(0).
                                    hand.size()
                            )
                    )
                    {
                        --selectedTile;
                    }


                    if (
                        selectedTile < 0
                    )
                    {
                        selectedTile =
                            0;
                    }
                }
            }


            // Triangle = self-drawn KONG
            // using selected tile.
            if (
                pressed &
                PSP_CTRL_TRIANGLE
            )
            {
                game.humanSelfKong(
                    selectedTile
                );
            }
        }


        sceDisplayWaitVblankStart();


        sceKernelDelayThread(
            1000
        );
    }


    sceKernelExitGame();


    return 0;
}