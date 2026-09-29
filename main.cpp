#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>

#include <stdint.h>
#include <stdio.h>

#include <map>
#include <string>
#include <vector>

#include "game_core.hpp"
#include "texture.hpp"
#include "audio.hpp"


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


static const uint32_t BLACK =
    0xFF080B0D;

static const uint32_t DARK =
    0xFF151C21;

static const uint32_t PANEL =
    0xFF222C32;

static const uint32_t FELT =
    0xFF235D32;

static const uint32_t FELT_DARK =
    0xFF173A21;

static const uint32_t BORDER =
    0xFF657178;

static const uint32_t WHITE =
    0xFFFFFFFF;

static const uint32_t MUTED =
    0xFFB9C0C5;

static const uint32_t GOLD =
    0xFF55D8FF;

static const uint32_t GREEN =
    0xFF72FF92;

static const uint32_t RED =
    0xFF7777FF;


static std::map<
    std::string,
    Texture
> gTiles;


static Texture gTitle;


static std::string gBasePath;


// =====================================================
// CALLBACK
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


void setupCallbacks()
{
    int thread =
        sceKernelCreateThread(
            "callback_thread",
            callbackThread,
            0x11,
            0xFA0,
            PSP_THREAD_ATTR_USER,
            0
        );


    if (
        thread >= 0
    )
    {
        sceKernelStartThread(
            thread,
            0,
            0
        );
    }
}


// =====================================================
// PATH
// =====================================================

std::string directoryFromArgv(
    int argc,
    char** argv
)
{
    if (
        argc <= 0 ||
        !argv ||
        !argv[0]
    )
    {
        return
            "ms0:/PSP/GAME/DEERSMAHJONG";
    }


    std::string path =
        argv[0];


    std::string::size_type slash =
        path.find_last_of(
            "/\\"
        );


    if (
        slash ==
        std::string::npos
    )
    {
        return ".";
    }


    return path.substr(
        0,
        slash
    );
}


std::string assetPath(
    const std::string& relative
)
{
    return
        gBasePath
        + "/assets/"
        + relative;
}


// =====================================================
// TEXT
// =====================================================

void text(
    int x,
    int y,
    uint32_t fg,
    uint32_t bg,
    const std::string& value
)
{
    pspDebugScreenSetTextColor(
        fg
    );

    pspDebugScreenSetBackColor(
        bg
    );

    pspDebugScreenSetXY(
        x / 8,
        y / 8
    );

    pspDebugScreenPrintf(
        "%s",
        value.c_str()
    );
}


// =====================================================
// LOAD ASSETS
// =====================================================

bool loadTile(
    const std::string& code
)
{
    Texture texture;


    if (
        !texture.load(
            assetPath(
                "tiles/doman/"
                + code
                + ".rgba"
            )
        )
    )
    {
        return false;
    }


    gTiles[code] =
        texture;


    return true;
}


bool loadAssets()
{
    static const char* codes[] = {
        "M1","M2","M3","M4","M5",
        "M6","M7","M8","M9",

        "D1","D2","D3","D4","D5",
        "D6","D7","D8","D9",

        "B1","B2","B3","B4","B5",
        "B6","B7","B8","B9",

        "E","S","W","N",

        "RD","GD","WD","J"
    };


    bool ok =
        true;


    for (
        unsigned int i = 0;
        i <
            sizeof(codes)
            /
            sizeof(codes[0]);
        ++i
    )
    {
        if (
            !loadTile(
                codes[i]
            )
        )
        {
            ok =
                false;
        }
    }


    if (
        !gTitle.load(
            assetPath(
                "ui/title.rgba"
            )
        )
    )
    {
        ok =
            false;
    }


    return ok;
}


// =====================================================
// TILE
// =====================================================

void tile(
    const std::string& code,
    int x,
    int y,
    int w,
    int h,
    bool selected
)
{
    if (selected)
    {
        fillRect(
            x - 2,
            y - 2,
            w + 4,
            h + 4,
            GOLD
        );
    }


    std::map<
        std::string,
        Texture
    >::iterator it =
        gTiles.find(
            code
        );


    if (
        it !=
        gTiles.end()
    )
    {
        drawTexture(
            it->second,
            x,
            y,
            w,
            h
        );

        return;
    }


    fillRect(
        x,
        y,
        w,
        h,
        0xFFE9E5D8
    );


    drawRect(
        x,
        y,
        w,
        h,
        BORDER
    );


    text(
        x + 3,
        y + h / 2 - 4,
        BLACK,
        0xFFE9E5D8,
        code
    );
}


// =====================================================
// TILE BACK
// =====================================================

void tileBack(
    int x,
    int y,
    int w,
    int h
)
{
    fillRect(
        x,
        y,
        w,
        h,
        0xFF40546C
    );


    drawRect(
        x,
        y,
        w,
        h,
        0xFF8197AE
    );


    drawRect(
        x + 3,
        y + 3,
        w - 6,
        h - 6,
        DARK
    );
}


// =====================================================
// MELDS
// =====================================================

void drawMelds(
    const Player& player,
    int x,
    int y
)
{
    int px =
        x;


    for (
        unsigned int m = 0;
        m <
            player.melds.size();
        ++m
    )
    {
        const Meld& meld =
            player.melds[m];


        for (
            unsigned int i = 0;
            i <
                meld.logical.size();
            ++i
        )
        {
            tile(
                meld.logical[i],
                px,
                y,
                18,
                25,
                false
            );


            px +=
                19;
        }


        px +=
            4;
    }
}


// =====================================================
// RIVER
// =====================================================

void drawRiver(
    const GameCore& game
)
{
    int x =
        118;

    int y =
        84;


    fillRect(
        x,
        y,
        244,
        78,
        FELT_DARK
    );


    drawRect(
        x,
        y,
        244,
        78,
        BORDER
    );


    const std::vector<
        DiscardEntry
    >& river =
        game.river();


    int count =
        river.size();


    int start =
        count > 24
            ? count - 24
            : 0;


    int index =
        0;


    for (
        int i = start;
        i < count;
        ++i
    )
    {
        int row =
            index / 12;

        int col =
            index % 12;


        tile(
            river[i].tile,
            x + 6 + col * 19,
            y + 7 + row * 32,
            18,
            27,
            false
        );


        ++index;
    }
}


// =====================================================
// BOT HANDS
// =====================================================

void drawBots(
    const GameCore& game
)
{
    const Player& west =
        game.player(2);


    int westCount =
        west.hand.size();


    if (westCount > 14)
        westCount = 14;


    int startX =
        240 -
        (
            westCount * 15
        ) / 2;


    for (
        int i = 0;
        i < westCount;
        ++i
    )
    {
        tileBack(
            startX + i * 15,
            49,
            14,
            25
        );
    }


    const Player& south =
        game.player(1);


    int southCount =
        south.hand.size();


    if (southCount > 14)
        southCount = 14;


    for (
        int i = 0;
        i < southCount;
        ++i
    )
    {
        tileBack(
            18,
            78 + i * 8,
            21,
            8
        );
    }


    const Player& north =
        game.player(3);


    int northCount =
        north.hand.size();


    if (northCount > 14)
        northCount = 14;


    for (
        int i = 0;
        i < northCount;
        ++i
    )
    {
        tileBack(
            441,
            78 + i * 8,
            21,
            8
        );
    }
}


// =====================================================
// HUMAN HAND
// =====================================================

void drawHuman(
    const GameCore& game,
    int selected
)
{
    const Player& human =
        game.player(0);


    int count =
        human.hand.size();


    if (count <= 0)
        return;


    int tileW =
        31;


    if (count > 14)
    {
        tileW =
            28;
    }


    int total =
        count * tileW;


    if (
        total > 466
    )
    {
        tileW =
            466 / count;


        total =
            tileW * count;
    }


    int x =
        (
            480 - total
        ) / 2;


    int baseY =
        220;


    for (
        int i = 0;
        i < count;
        ++i
    )
    {
        bool chosen =
            i == selected;


        int y =
            chosen
                ? baseY - 6
                : baseY;


        tile(
            human.hand[i],
            x + i * tileW,
            y,
            tileW - 2,
            42,
            chosen
        );
    }


    drawMelds(
        human,
        42,
        184
    );
}


// =====================================================
// CLAIM WINDOW
// =====================================================

void drawClaim(
    const GameCore& game
)
{
    const ClaimPrompt& prompt =
        game.claimPrompt();


    if (!prompt.active)
        return;


    fillRect(
        80,
        91,
        320,
        83,
        0xFF302A25
    );


    drawRect(
        80,
        91,
        320,
        83,
        GOLD
    );


    text(
        96,
        98,
        GOLD,
        0xFF302A25,
        "YOUR DECISION"
    );


    std::string line =
        game.player(
            (int)prompt.from
        ).name
        + " -> "
        + prompt.tile;


    text(
        96,
        113,
        WHITE,
        0xFF302A25,
        line
    );


    text(
        96,
        129,
        MUTED,
        0xFF302A25,
        "O DRAW/PASS"
    );


    if (prompt.mahjong)
    {
        text(
            220,
            129,
            RED,
            0xFF302A25,
            "X MAHJONG"
        );
    }


    if (!prompt.chows.empty())
    {
        text(
            96,
            145,
            WHITE,
            0xFF302A25,
            "SQUARE CHOW"
        );
    }


    if (prompt.pung)
    {
        text(
            205,
            145,
            GREEN,
            0xFF302A25,
            "TRIANGLE PUNG"
        );
    }


    if (prompt.kong)
    {
        text(
            326,
            145,
            GOLD,
            0xFF302A25,
            "L KONG"
        );
    }
}


// =====================================================
// RESULT
// =====================================================

void drawResult(
    const GameCore& game
)
{
    if (!game.handOver())
        return;


    fillRect(
        72,
        90,
        336,
        92,
        0xFF302A25
    );


    drawRect(
        72,
        90,
        336,
        92,
        GOLD
    );


    if (
        game.hasWinner()
    )
    {
        text(
            184,
            104,
            GOLD,
            0xFF302A25,
            "MAHJONG"
        );


        std::string winner =
            "WINNER: "
            + game.player(
                (int)game.winner()
            ).name;


        text(
            104,
            126,
            WHITE,
            0xFF302A25,
            winner
        );


        std::string method =
            "METHOD: "
            + game.winningMethod();


        text(
            104,
            143,
            GREEN,
            0xFF302A25,
            method
        );
    }

    else
    {
        text(
            160,
            123,
            MUTED,
            0xFF302A25,
            "DRAWN HAND"
        );
    }


    text(
        164,
        162,
        WHITE,
        0xFF302A25,
        "START NEW HAND"
    );
}


// =====================================================
// RENDER
// =====================================================

void render(
    const GameCore& game,
    int selected,
    const AudioLoop& music
)
{
    clearScreen(
        BLACK
    );


    // TOP:
    // ONLY TITLE

    if (
        gTitle.valid()
    )
    {
        drawTexture(
            gTitle,
            15,
            2,
            450,
            46
        );
    }


    fillRect(
        12,
        49,
        456,
        161,
        FELT
    );


    drawRect(
        12,
        49,
        456,
        161,
        BORDER
    );


    drawBots(
        game
    );


    drawRiver(
        game
    );


    char wall[32];


    snprintf(
        wall,
        sizeof(wall),
        "WALL %d",
        game.wallCount()
    );


    text(
        205,
        166,
        MUTED,
        FELT,
        wall
    );


    drawHuman(
        game,
        selected
    );


    if (
        game.humanNeedsDiscard() &&
        !game.claimPrompt().active &&
        !game.handOver()
    )
    {
        text(
            314,
            191,
            MUTED,
            FELT,
            "< > SELECT"
        );


        text(
            314,
            201,
            WHITE,
            FELT,
            "X DISCARD"
        );
    }


    drawClaim(
        game
    );


    drawResult(
        game
    );


    fillRect(
        0,
        262,
        480,
        10,
        DARK
    );


    std::string status =
        game.message();


    text(
        4,
        263,
        GREEN,
        DARK,
        status
    );


    text(
        390,
        263,
        music.enabled()
            ? GOLD
            : MUTED,
        DARK,
        music.enabled()
            ? "MUSIC ON"
            : "MUSIC OFF"
    );
}


// =====================================================
// MAIN
// =====================================================

int main(
    int argc,
    char** argv
)
{
    setupCallbacks();


    videoInit();


    sceCtrlSetSamplingCycle(
        0
    );


    sceCtrlSetSamplingMode(
        PSP_CTRL_MODE_DIGITAL
    );


    gBasePath =
        directoryFromArgv(
            argc,
            argv
        );


    loadAssets();


    AudioLoop music;


    music.load(
        assetPath(
            "audio/background.wav"
        )
    );


    music.start();


    GameCore game;


    uint32_t seed =
        (uint32_t)
        sceKernelGetSystemTimeLow();


    game.newHand(
        seed
    );


    int selected =
        0;


    unsigned int previous =
        0;


    while (1)
    {
        game.advance();


        int count =
            game.player(0).
                hand.size();


        if (count <= 0)
        {
            selected =
                0;
        }

        else
        {
            if (
                selected >= count
            )
            {
                selected =
                    count - 1;
            }


            if (
                selected < 0
            )
            {
                selected =
                    0;
            }
        }


        render(
            game,
            selected,
            music
        );


        SceCtrlData pad;


        sceCtrlPeekBufferPositive(
            &pad,
            1
        );


        unsigned int pressed =
            pad.Buttons
            & ~previous;


        previous =
            pad.Buttons;


        if (
            pressed &
            PSP_CTRL_START
        )
        {
            seed =
                (uint32_t)
                sceKernelGetSystemTimeLow();


            game.newHand(
                seed
            );


            selected =
                0;
        }


        if (
            pressed &
            PSP_CTRL_SELECT
        )
        {
            music.toggle();
        }


        if (
            game.claimPrompt().active &&
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


        else if (
            game.humanNeedsDiscard() &&
            !game.handOver()
        )
        {
            count =
                game.player(0).
                    hand.size();


            if (
                count > 0 &&
                (
                    pressed &
                    PSP_CTRL_LEFT
                )
            )
            {
                --selected;


                if (
                    selected < 0
                )
                {
                    selected =
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
                ++selected;


                if (
                    selected >=
                    count
                )
                {
                    selected =
                        0;
                }
            }


            if (
                pressed &
                PSP_CTRL_CROSS
            )
            {
                game.humanDiscard(
                    selected
                );
            }


            if (
                pressed &
                PSP_CTRL_TRIANGLE
            )
            {
                game.humanSelfKong(
                    selected
                );
            }
        }


        sceDisplayWaitVblankStart();


        sceKernelDelayThread(
            1000
        );
    }


    return 0;
}