#include "game_core.hpp"

#include <algorithm>

namespace {
const unsigned int BOT_THINK_MS = 1500;
const unsigned int DISCARD_SHOW_MS = 1200;
}


GameCore::GameCore()
    : turn_(EAST),
      lastDiscarder_(EAST),
      awaitingHumanDiscard_(false),
      resolvePending_(false),
      handOver_(false),
      hasWinner_(false),
      winner_(EAST),
      rngState_(1),
      waitMs_(0),
      freshDelay_(false),
      botClaimDiscardPending_(false),
      highlightedDiscard_(-1)
{
    players_[0].seat = EAST;
    players_[0].name = "YOU / EAST";

    players_[1].seat = SOUTH;
    players_[1].name = "BOT LEFT / SOUTH";

    players_[2].seat = WEST;
    players_[2].name = "BOT OPPOSITE / WEST";

    players_[3].seat = NORTH;
    players_[3].name = "BOT RIGHT / NORTH";

    for (int i = 0; i < 4; ++i)
    {
        players_[i].mustDiscard = false;
        players_[i].mrh = false;
    }
}


// =====================================================
// BASIC HELPERS
// =====================================================

int GameCore::seatIndex(
    Seat seat
) const
{
    return static_cast<int>(seat);
}

Seat GameCore::nextSeat(
    Seat seat
) const
{
    return static_cast<Seat>(
        (seatIndex(seat) + 1) % 4
    );
}

std::string GameCore::seatName(
    Seat seat
) const
{
    return players_[seatIndex(seat)].name;
}


ClaimPermission GameCore::claimPermission(
    Seat discarder,
    Seat claimant
) const
{
    int d = seatIndex(discarder);
    int c = seatIndex(claimant);

    // CLAIMANT POV:
    //
    // RIGHT     -> CHOW
    // LEFT      -> PUNG
    // OPPOSITE  -> PUNG

    if (c == (d + 1) % 4)
        return CLAIM_CHOW;

    if (c == (d + 3) % 4)
        return CLAIM_PUNG;

    if (c == (d + 2) % 4)
        return CLAIM_PUNG;

    return CLAIM_NONE;
}


// =====================================================
// TILE HELPERS
// =====================================================

bool GameCore::isSuited(
    const std::string& tile
) const
{
    return (
        tile.size() == 2 &&
        (
            tile[0] == 'M' ||
            tile[0] == 'D' ||
            tile[0] == 'B'
        ) &&
        tile[1] >= '1' &&
        tile[1] <= '9'
    );
}


int GameCore::tileRank(
    const std::string& tile
) const
{
    if (isSuited(tile))
    {
        int base = 0;

        if (tile[0] == 'M')
            base = 0;

        else if (tile[0] == 'D')
            base = 20;

        else
            base = 40;

        return base + (tile[1] - '0');
    }

    if (tile == "E")  return 61;
    if (tile == "S")  return 62;
    if (tile == "W")  return 63;
    if (tile == "N")  return 64;

    if (tile == "RD") return 71;
    if (tile == "GD") return 72;
    if (tile == "WD") return 73;

    if (tile == "J")  return 80;

    return 999;
}


void GameCore::sortHand(
    Player& player
)
{
    for (size_t i = 0;
         i < player.hand.size();
         ++i)
    {
        for (size_t j = i + 1;
             j < player.hand.size();
             ++j)
        {
            if (
                tileRank(player.hand[j]) <
                tileRank(player.hand[i])
            )
            {
                std::swap(
                    player.hand[i],
                    player.hand[j]
                );
            }
        }
    }
}


int GameCore::countTile(
    const std::vector<std::string>& hand,
    const std::string& tile
) const
{
    int total = 0;

    for (size_t i = 0;
         i < hand.size();
         ++i)
    {
        if (hand[i] == tile)
            ++total;
    }

    return total;
}


bool GameCore::removeOne(
    std::vector<std::string>& hand,
    const std::string& tile
) const
{
    for (std::vector<std::string>::iterator it =
             hand.begin();
         it != hand.end();
         ++it)
    {
        if (*it == tile)
        {
            hand.erase(it);

            return true;
        }
    }

    return false;
}


// =====================================================
// RNG
// =====================================================

uint32_t GameCore::nextRandom()
{
    rngState_ =
        (rngState_ * 1664525u)
        + 1013904223u;

    return rngState_;
}


// =====================================================
// WALL
// =====================================================

void GameCore::buildWall()
{
    wall_.clear();

    const char suits[3] = {
        'M',
        'D',
        'B'
    };

    for (int s = 0;
         s < 3;
         ++s)
    {
        for (int number = 1;
             number <= 9;
             ++number)
        {
            std::string tile;

            tile += suits[s];

            tile += static_cast<char>(
                '0' + number
            );

            for (int copy = 0;
                 copy < 4;
                 ++copy)
            {
                wall_.push_back(tile);
            }
        }
    }


    const char* honors[] = {
        "E",
        "S",
        "W",
        "N",
        "RD",
        "GD",
        "WD"
    };

    for (int h = 0;
         h < 7;
         ++h)
    {
        for (int copy = 0;
             copy < 4;
             ++copy)
        {
            wall_.push_back(
                honors[h]
            );
        }
    }


    // Four physical Jokers.
    for (int copy = 0;
         copy < 4;
         ++copy)
    {
        wall_.push_back("J");
    }
}


void GameCore::shuffleWall()
{
    if (wall_.empty())
        return;

    for (int i =
             static_cast<int>(
                 wall_.size()
             ) - 1;
         i > 0;
         --i)
    {
        int j =
            static_cast<int>(
                nextRandom()
                %
                static_cast<uint32_t>(
                    i + 1
                )
            );

        std::swap(
            wall_[i],
            wall_[j]
        );
    }
}


// =====================================================
// NEW HAND
// =====================================================

void GameCore::newHand(
    uint32_t seed
)
{
    if (seed == 0)
        seed = 0x13572468u;

    rngState_ =
        seed;

    wall_.clear();
    river_.clear();
    waitMs_ = 0;
    freshDelay_ = false;
    botClaimDiscardPending_ = false;
    highlightedDiscard_ = -1;

    lastDiscard_.clear();

    turn_ =
        EAST;

    lastDiscarder_ =
        EAST;

    awaitingHumanDiscard_ =
        false;

    resolvePending_ =
        false;

    handOver_ =
        false;

    hasWinner_ =
        false;

    winner_ =
        EAST;

    winningMethod_.clear();

    prompt_ =
        ClaimPrompt();

    message_ =
        "New hand.";


    for (int i = 0;
         i < 4;
         ++i)
    {
        players_[i].hand.clear();
        players_[i].melds.clear();

        players_[i].mustDiscard =
            false;

        players_[i].mrh =
            false;

        players_[i].justDrew.clear();

        players_[i].
            claimDiscardLock.clear();
    }


    buildWall();
    shuffleWall();


    // 13 tiles each.
    for (int round = 0;
         round < 13;
         ++round)
    {
        for (int p = 0;
             p < 4;
             ++p)
        {
            drawOne(
                static_cast<Seat>(p),
                false
            );
        }
    }


    // EAST starts with 14.
    drawOne(
        EAST,
        false
    );


    players_[0].mustDiscard =
        true;

    awaitingHumanDiscard_ =
        true;

    turn_ =
        EAST;

    message_ =
        "Your opening discard.";
}


// =====================================================
// DRAW
// =====================================================

bool GameCore::drawOne(
    Seat seat,
    bool announce
)
{
    if (wall_.empty())
    {
        handOver_ =
            true;

        hasWinner_ =
            false;

        message_ =
            "Wall empty - drawn hand.";

        return false;
    }


    Player& player =
        players_[
            seatIndex(seat)
        ];


    std::string tile =
        wall_.back();

    wall_.pop_back();


    player.hand.push_back(tile);

    player.justDrew =
        tile;

    player.mustDiscard =
        true;

    sortHand(player);


    if (announce)
    {
        message_ =
            seatName(seat)
            + " draws "
            + tile;
    }

    return true;
}


// =====================================================
// CHOW
// =====================================================

std::vector<std::vector<std::string> >
GameCore::possibleChows(
    const Player& player,
    const std::string& discard
) const
{
    std::vector<
        std::vector<std::string>
    > result;


    if (!isSuited(discard))
        return result;


    char suit =
        discard[0];

    int rank =
        discard[1] - '0';


    int minimum =
        std::max(
            1,
            rank - 2
        );

    int maximum =
        std::min(
            rank,
            7
        );


    for (int start = minimum;
         start <= maximum;
         ++start)
    {
        std::vector<std::string>
            logical;

        for (int n = start;
             n <= start + 2;
             ++n)
        {
            std::string tile;

            tile += suit;

            tile +=
                static_cast<char>(
                    '0' + n
                );

            logical.push_back(tile);
        }


        std::vector<std::string>
            remaining =
                player.hand;


        bool discardUsed =
            false;

        bool valid =
            true;


        for (size_t i = 0;
             i < logical.size();
             ++i)
        {
            const std::string& wanted =
                logical[i];


            if (
                wanted == discard &&
                !discardUsed
            )
            {
                discardUsed =
                    true;

                continue;
            }


            if (
                removeOne(
                    remaining,
                    wanted
                )
            )
            {
                continue;
            }


            // Joker may fill CHOW.
            if (
                removeOne(
                    remaining,
                    "J"
                )
            )
            {
                continue;
            }


            valid =
                false;

            break;
        }


        if (valid)
        {
            result.push_back(
                logical
            );
        }
    }


    return result;
}


// =====================================================
// PUNG / KONG
// =====================================================

bool GameCore::canPung(
    const Player& player,
    const std::string& discard
) const
{
    if (discard == "J")
        return false;


    int exact =
        countTile(
            player.hand,
            discard
        );

    int jokers =
        countTile(
            player.hand,
            "J"
        );


    return (
        exact + jokers >= 2
    );
}


bool GameCore::canKong(
    const Player& player,
    const std::string& discard
) const
{
    if (discard == "J")
        return false;


    // Joker cannot create KONG.
    return (
        countTile(
            player.hand,
            discard
        )
        >= 3
    );
}


// =====================================================
// WIN SOLVER
// =====================================================

bool GameCore::buildSetsRecursive(
    std::vector<std::string> tiles,
    int setsNeeded
) const
{
    if (setsNeeded == 0)
        return tiles.empty();


    if (
        static_cast<int>(
            tiles.size()
        )
        != setsNeeded * 3
    )
    {
        return false;
    }


    // Sort tiles.
    for (size_t i = 0;
         i < tiles.size();
         ++i)
    {
        for (size_t j = i + 1;
             j < tiles.size();
             ++j)
        {
            if (
                tileRank(tiles[j]) <
                tileRank(tiles[i])
            )
            {
                std::swap(
                    tiles[i],
                    tiles[j]
                );
            }
        }
    }


    std::string first;


    for (size_t i = 0;
         i < tiles.size();
         ++i)
    {
        if (tiles[i] != "J")
        {
            first =
                tiles[i];

            break;
        }
    }


    // Do not create a pure-Joker set.
    if (first.empty())
        return false;


    // =================================================
    // PUNG
    // =================================================

    int same =
        countTile(
            tiles,
            first
        );

    int jokers =
        countTile(
            tiles,
            "J"
        );


    int maxReal =
        std::min(
            3,
            same
        );


    for (int real = maxReal;
         real >= 1;
         --real)
    {
        int jokerNeeded =
            3 - real;


        if (
            jokerNeeded >
            jokers
        )
        {
            continue;
        }


        std::vector<std::string>
            remaining =
                tiles;


        bool valid =
            true;


        for (int i = 0;
             i < real;
             ++i)
        {
            if (
                !removeOne(
                    remaining,
                    first
                )
            )
            {
                valid =
                    false;

                break;
            }
        }


        if (!valid)
            continue;


        for (int i = 0;
             i < jokerNeeded;
             ++i)
        {
            if (
                !removeOne(
                    remaining,
                    "J"
                )
            )
            {
                valid =
                    false;

                break;
            }
        }


        if (
            valid &&
            buildSetsRecursive(
                remaining,
                setsNeeded - 1
            )
        )
        {
            return true;
        }
    }


    // =================================================
    // CHOW
    // =================================================

    if (isSuited(first))
    {
        char suit =
            first[0];

        int rank =
            first[1] - '0';


        int minimum =
            std::max(
                1,
                rank - 2
            );

        int maximum =
            std::min(
                rank,
                7
            );


        for (int start = minimum;
             start <= maximum;
             ++start)
        {
            std::vector<std::string>
                remaining =
                    tiles;


            bool valid =
                true;

            bool firstUsed =
                false;


            for (int n = start;
                 n <= start + 2;
                 ++n)
            {
                std::string wanted;

                wanted += suit;

                wanted +=
                    static_cast<char>(
                        '0' + n
                    );


                if (
                    wanted == first &&
                    !firstUsed
                )
                {
                    if (
                        !removeOne(
                            remaining,
                            first
                        )
                    )
                    {
                        valid =
                            false;

                        break;
                    }


                    firstUsed =
                        true;

                    continue;
                }


                if (
                    removeOne(
                        remaining,
                        wanted
                    )
                )
                {
                    continue;
                }


                if (
                    removeOne(
                        remaining,
                        "J"
                    )
                )
                {
                    continue;
                }


                valid =
                    false;

                break;
            }


            if (
                valid &&
                firstUsed &&
                buildSetsRecursive(
                    remaining,
                    setsNeeded - 1
                )
            )
            {
                return true;
            }
        }
    }


    return false;
}


// =====================================================
// MAHJONG CHECK
// =====================================================

bool GameCore::canMahjong(
    const Player& player
) const
{
    int exposedSets =
        static_cast<int>(
            player.melds.size()
        );


    int setsNeeded =
        4 - exposedSets;


    if (setsNeeded < 0)
        return false;


    int expectedTiles =
        (setsNeeded * 3) + 2;


    if (
        static_cast<int>(
            player.hand.size()
        )
        != expectedTiles
    )
    {
        return false;
    }


    const std::vector<std::string>& hand =
        player.hand;


    // =================================================
    // JOKER + JOKER PAIR
    // =================================================

    if (
        countTile(
            hand,
            "J"
        ) >= 2
    )
    {
        std::vector<std::string>
            remaining =
                hand;


        removeOne(
            remaining,
            "J"
        );

        removeOne(
            remaining,
            "J"
        );


        if (
            buildSetsRecursive(
                remaining,
                setsNeeded
            )
        )
        {
            return true;
        }
    }


    // =================================================
    // REAL PAIR / REAL + JOKER
    // =================================================

    std::vector<std::string>
        checked;


    for (size_t i = 0;
         i < hand.size();
         ++i)
    {
        std::string tile =
            hand[i];


        if (tile == "J")
            continue;


        bool alreadyChecked =
            false;


        for (size_t c = 0;
             c < checked.size();
             ++c)
        {
            if (checked[c] == tile)
            {
                alreadyChecked =
                    true;

                break;
            }
        }


        if (alreadyChecked)
            continue;


        checked.push_back(tile);


        // Real + real pair.
        if (
            countTile(
                hand,
                tile
            ) >= 2
        )
        {
            std::vector<std::string>
                remaining =
                    hand;


            removeOne(
                remaining,
                tile
            );

            removeOne(
                remaining,
                tile
            );


            if (
                buildSetsRecursive(
                    remaining,
                    setsNeeded
                )
            )
            {
                return true;
            }
        }


        // Real + Joker logical pair.
        if (
            countTile(
                hand,
                tile
            ) >= 1 &&
            countTile(
                hand,
                "J"
            ) >= 1
        )
        {
            std::vector<std::string>
                remaining =
                    hand;


            removeOne(
                remaining,
                tile
            );

            removeOne(
                remaining,
                "J"
            );


            if (
                buildSetsRecursive(
                    remaining,
                    setsNeeded
                )
            )
            {
                return true;
            }
        }
    }


    return false;
}


bool GameCore::canMahjongWithExtra(
    const Player& player,
    const std::string& extra
) const
{
    Player copy =
        player;


    copy.hand.push_back(
        extra
    );


    return canMahjong(
        copy
    );
}


// =====================================================
// SELF DRAW RULE
// =====================================================

bool GameCore::canSelfDrawMahjong(
    const Player& player
) const
{
    // Web house rule: with three formed sets and an already complete 3+2
    // before this draw, the newly drawn tile is only the winning trigger.
    if (player.mrh && player.melds.size() == 3 &&
        player.hand.size() == 6 && !player.justDrew.empty())
    {
        Player beforeDraw = player;
        if (removeOne(beforeDraw.hand, player.justDrew) && canMahjong(beforeDraw))
            return true;
    }

    if (!canMahjong(player))
        return false;


    int revealed =
        static_cast<int>(
            player.melds.size()
        );


    // No revealed sets:
    // self draw allowed.
    if (revealed == 0)
        return true;


    // Exactly one revealed set:
    // self draw NOT allowed.
    if (revealed == 1)
        return false;


    // 2+ revealed sets:
    // requires MRH.
    return player.mrh;
}


// =====================================================
// CLAIM OPTIONS
// =====================================================

ClaimOptions GameCore::inspectClaim(
    Seat claimant
) const
{
    ClaimOptions options;


    if (
        lastDiscard_.empty() ||
        claimant ==
            lastDiscarder_
    )
    {
        return options;
    }


    const Player& player =
        players_[
            seatIndex(claimant)
        ];


    ClaimPermission permission =
        claimPermission(
            lastDiscarder_,
            claimant
        );


    // Winning exception:
    // may claim Mahjong from ANY opponent.
    options.mahjong =
        canMahjongWithExtra(
            player,
            lastDiscard_
        );


    if (
        permission ==
        CLAIM_CHOW
    )
    {
        options.chows =
            possibleChows(
                player,
                lastDiscard_
            );
    }


    if (
        permission ==
        CLAIM_PUNG
    )
    {
        options.pung =
            canPung(
                player,
                lastDiscard_
            );
    }


    // KONG from ANY opponent.
    options.kong =
        canKong(
            player,
            lastDiscard_
        );


    return options;
}


std::vector<ClaimOffer>
GameCore::findClaims() const
{
    std::vector<ClaimOffer>
        result;


    for (int i = 0;
         i < 4;
         ++i)
    {
        Seat claimant =
            static_cast<Seat>(i);


        if (
            claimant ==
            lastDiscarder_
        )
        {
            continue;
        }


        ClaimOptions options =
            inspectClaim(
                claimant
            );


        if (
            !options.mahjong &&
            !options.kong &&
            !options.pung &&
            options.chows.empty()
        )
        {
            continue;
        }


        ClaimOffer offer;

        offer.claimant =
            claimant;

        offer.permission =
            claimPermission(
                lastDiscarder_,
                claimant
            );

        offer.options =
            options;


        result.push_back(
            offer
        );
    }


    return result;
}


// =====================================================
// DISCARD
// =====================================================

bool GameCore::makeDiscard(
    Seat seat,
    int index
)
{
    Player& player =
        players_[
            seatIndex(seat)
        ];


    if (
        index < 0 ||
        index >=
            static_cast<int>(
                player.hand.size()
            )
    )
    {
        return false;
    }


    std::string tile =
        player.hand[index];


    if (
        !player.
            claimDiscardLock.empty() &&
        tile ==
            player.claimDiscardLock
    )
    {
        message_ =
            "That value is locked after claim.";

        return false;
    }


    player.hand.erase(
        player.hand.begin()
        + index
    );


    player.mustDiscard =
        false;

    player.justDrew.clear();


    // Lock clears after LEGAL discard.
    player.claimDiscardLock.clear();


    lastDiscard_ =
        tile;

    lastDiscarder_ =
        seat;


    DiscardEntry entry;

    entry.seat =
        seat;

    entry.tile =
        tile;


    river_.push_back(
        entry
    );

    highlightedDiscard_ = static_cast<int>(river_.size()) - 1;
    waitMs_ = DISCARD_SHOW_MS;
    freshDelay_ = true;


    resolvePending_ =
        true;


    message_ =
        seatName(seat)
        + " discards "
        + tile;


    return true;
}


// =====================================================
// CONSUME CLAIMED TILE
// =====================================================

void GameCore::consumeLastDiscard()
{
    if (
        !river_.empty() &&
        river_.back().tile ==
            lastDiscard_
    )
    {
        river_.pop_back();
        // A consumed tile is no longer in the river. Do not highlight the
        // older tile that now happens to be at the back of the vector.
        highlightedDiscard_ = -1;
    }


    lastDiscard_.clear();

    resolvePending_ =
        false;
}


// =====================================================
// CHOW EXECUTION
// =====================================================

bool GameCore::doChow(
    Seat claimant,
    const std::vector<std::string>& sequence
)
{
    if (
        claimPermission(
            lastDiscarder_,
            claimant
        )
        != CLAIM_CHOW
    )
    {
        return false;
    }


    Player& player =
        players_[
            seatIndex(claimant)
        ];


    std::string claimed =
        lastDiscard_;


    std::vector<
        std::vector<std::string>
    > legal =
        possibleChows(
            player,
            claimed
        );


    bool found =
        false;


    for (size_t i = 0;
         i < legal.size();
         ++i)
    {
        if (legal[i] == sequence)
        {
            found =
                true;

            break;
        }
    }


    if (!found)
        return false;


    std::vector<std::string>
        physical;


    physical.push_back(
        claimed
    );


    bool claimedUsed =
        false;


    for (size_t i = 0;
         i < sequence.size();
         ++i)
    {
        const std::string& wanted =
            sequence[i];


        if (
            wanted == claimed &&
            !claimedUsed
        )
        {
            claimedUsed =
                true;

            continue;
        }


        if (
            removeOne(
                player.hand,
                wanted
            )
        )
        {
            physical.push_back(
                wanted
            );
        }

        else if (
            removeOne(
                player.hand,
                "J"
            )
        )
        {
            physical.push_back(
                "J"
            );
        }

        else
        {
            return false;
        }
    }


    Meld meld;

    meld.kind =
        "CHOW";

    meld.physical =
        physical;

    meld.logical =
        sequence;


    player.melds.push_back(
        meld
    );


    consumeLastDiscard();


    player.claimDiscardLock =
        claimed;

    player.mustDiscard =
        true;

    player.justDrew.clear();


    turn_ =
        claimant;


    sortHand(player);


    return true;
}


// =====================================================
// PUNG EXECUTION
// =====================================================

bool GameCore::doPung(
    Seat claimant
)
{
    if (
        claimPermission(
            lastDiscarder_,
            claimant
        )
        != CLAIM_PUNG
    )
    {
        return false;
    }


    Player& player =
        players_[
            seatIndex(claimant)
        ];


    std::string claimed =
        lastDiscard_;


    if (
        !canPung(
            player,
            claimed
        )
    )
    {
        return false;
    }


    std::vector<std::string>
        physical;


    physical.push_back(
        claimed
    );


    for (int i = 0;
         i < 2;
         ++i)
    {
        if (
            removeOne(
                player.hand,
                claimed
            )
        )
        {
            physical.push_back(
                claimed
            );
        }

        else if (
            removeOne(
                player.hand,
                "J"
            )
        )
        {
            physical.push_back(
                "J"
            );
        }

        else
        {
            return false;
        }
    }


    Meld meld;

    meld.kind =
        "PUNG";

    meld.physical =
        physical;

    meld.logical.push_back(
        claimed
    );

    meld.logical.push_back(
        claimed
    );

    meld.logical.push_back(
        claimed
    );


    player.melds.push_back(
        meld
    );


    consumeLastDiscard();


    player.claimDiscardLock =
        claimed;

    player.mustDiscard =
        true;

    player.justDrew.clear();


    turn_ =
        claimant;


    sortHand(player);


    return true;
}


// =====================================================
// KONG FROM DISCARD
// =====================================================

bool GameCore::doKongFromDiscard(
    Seat claimant
)
{
    Player& player =
        players_[
            seatIndex(claimant)
        ];


    std::string claimed =
        lastDiscard_;


    if (
        !canKong(
            player,
            claimed
        )
    )
    {
        return false;
    }


    std::vector<std::string>
        physical;


    physical.push_back(
        claimed
    );


    for (int i = 0;
         i < 3;
         ++i)
    {
        if (
            !removeOne(
                player.hand,
                claimed
            )
        )
        {
            return false;
        }


        physical.push_back(
            claimed
        );
    }


    Meld meld;

    meld.kind =
        "KONG";

    meld.physical =
        physical;


    for (int i = 0;
         i < 4;
         ++i)
    {
        meld.logical.push_back(
            claimed
        );
    }


    player.melds.push_back(
        meld
    );


    consumeLastDiscard();


    player.claimDiscardLock.clear();

    player.mustDiscard =
        false;

    player.justDrew.clear();


    // Replacement tile.
    if (
        !drawOne(
            claimant,
            false
        )
    )
    {
        return false;
    }


    // HOUSE RULE:
    // KONG -> replacement tile -> NO DISCARD
    // -> immediately next player.
    player.justDrew.clear();

    player.mustDiscard =
        false;


    turn_ =
        nextSeat(
            claimant
        );


    resolvePending_ =
        false;


    message_ =
        seatName(claimant)
        + " forms KONG - no discard.";


    waitMs_ = BOT_THINK_MS;
    freshDelay_ = true;

    return true;
}


// =====================================================
// SELF KONG
// =====================================================

std::vector<std::string>
GameCore::selfKongs(
    const Player& player
) const
{
    std::vector<std::string>
        result;


    for (size_t i = 0;
         i < player.hand.size();
         ++i)
    {
        const std::string& tile =
            player.hand[i];


        if (tile == "J")
            continue;


        bool already =
            false;


        for (size_t x = 0;
             x < result.size();
             ++x)
        {
            if (result[x] == tile)
            {
                already =
                    true;

                break;
            }
        }


        if (already)
            continue;


        if (
            countTile(
                player.hand,
                tile
            ) >= 4
        )
        {
            result.push_back(
                tile
            );
        }
    }


    return result;
}


bool GameCore::doSelfKong(
    Seat seat,
    const std::string& tile
)
{
    Player& player =
        players_[
            seatIndex(seat)
        ];


    if (
        tile == "J" ||
        countTile(
            player.hand,
            tile
        ) < 4
    )
    {
        return false;
    }


    Meld meld;

    meld.kind =
        "KONG";


    for (int i = 0;
         i < 4;
         ++i)
    {
        removeOne(
            player.hand,
            tile
        );

        meld.physical.push_back(
            tile
        );

        meld.logical.push_back(
            tile
        );
    }


    player.melds.push_back(
        meld
    );


    if (
        !drawOne(
            seat,
            false
        )
    )
    {
        return false;
    }


    // Same house rule.
    player.justDrew.clear();

    player.mustDiscard =
        false;

    player.claimDiscardLock.clear();


    awaitingHumanDiscard_ =
        false;


    turn_ =
        nextSeat(seat);


    resolvePending_ =
        false;


    message_ =
        seatName(seat)
        + " forms self KONG - no discard.";


    waitMs_ = BOT_THINK_MS;
    freshDelay_ = true;

    return true;
}


// =====================================================
// MAHJONG
// =====================================================

void GameCore::declareMahjong(
    Seat seat,
    const std::string& method
)
{
    handOver_ =
        true;

    hasWinner_ =
        true;

    winner_ =
        seat;

    turn_ =
        seat;

    winningMethod_ =
        method;

    awaitingHumanDiscard_ =
        false;

    resolvePending_ =
        false;

    prompt_ =
        ClaimPrompt();


    players_[
        seatIndex(seat)
    ].mustDiscard =
        false;


    message_ =
        seatName(seat)
        + " - MAHJONG - "
        + method;
}


// =====================================================
// HUMAN DISCARD
// =====================================================

bool GameCore::humanCanDeclareMrh() const
{
    return !handOver_ && turn_ == EAST && !prompt_.active &&
           !resolvePending_ && !players_[0].mrh;
}

bool GameCore::humanDeclareMrh()
{
    if (!humanCanDeclareMrh())
        return false;

    players_[0].mrh = true;
    message_ = "MRH declared: win +12 / fail -20.";

    // PSP already awards eligible self-draw wins automatically. Recheck now:
    // the current completed hand may have been blocked only by missing MRH.
    if (canSelfDrawMahjong(players_[0]))
        declareMahjong(EAST, "SELF DRAW / MRH");
    return true;
}

int GameCore::humanMrhAdjustment() const
{
    if (!handOver_ || !players_[0].mrh)
        return 0;
    // Separate MRH adjustment; this PSP version has no full score engine.
    return hasWinner_ && winner_ == EAST ? 12 : -20;
}

bool GameCore::humanDiscard(
    int index
)
{
    if (
        handOver_ ||
        prompt_.active ||
        !awaitingHumanDiscard_ ||
        turn_ != EAST
    )
    {
        return false;
    }


    if (
        !makeDiscard(
            EAST,
            index
        )
    )
    {
        return false;
    }


    awaitingHumanDiscard_ =
        false;


    return true;
}


// =====================================================
// HUMAN DRAW / PASS
// =====================================================

bool GameCore::humanPass()
{
    if (
        handOver_ ||
        !prompt_.active
    )
    {
        return false;
    }


    prompt_ =
        ClaimPrompt();


    // The old discarded tile remains in the river,
    // but its claim window is resolved.
    lastDiscard_.clear();

    resolvePending_ =
        false;


    turn_ =
        EAST;


    Player& human =
        players_[0];


    human.claimDiscardLock.clear();

    human.justDrew.clear();


    // HOUSE RULE:
    // PASS -> draw exactly one -> HUMAN KEEPS TURN.
    if (
        !drawOne(
            EAST,
            true
        )
    )
    {
        return false;
    }


    if (
        canSelfDrawMahjong(
            human
        )
    )
    {
        declareMahjong(
            EAST,
            "SELF DRAW AFTER PASS"
        );

        return true;
    }


    awaitingHumanDiscard_ =
        true;


    message_ =
        "DRAW/PASS - discard one tile.";


    return true;
}


// =====================================================
// HUMAN CHOW
// =====================================================

bool GameCore::humanClaimChow()
{
    if (
        !prompt_.active ||
        prompt_.chows.empty()
    )
    {
        return false;
    }


    int index =
        prompt_.selectedChow;


    if (
        index < 0 ||
        index >=
            static_cast<int>(
                prompt_.chows.size()
            )
    )
    {
        index = 0;
    }


    std::vector<std::string>
        chosen =
            prompt_.chows[index];


    prompt_ =
        ClaimPrompt();


    if (
        !doChow(
            EAST,
            chosen
        )
    )
    {
        return false;
    }


    Player& human =
        players_[0];


    // Claim itself may finish the hand.
    if (
        canMahjong(
            human
        )
    )
    {
        declareMahjong(
            EAST,
            "CHOW CLAIM"
        );

        return true;
    }


    awaitingHumanDiscard_ =
        true;


    message_ =
        "CHOW claimed - discard one tile.";


    return true;
}


// =====================================================
// HUMAN PUNG
// =====================================================

bool GameCore::humanClaimPung()
{
    if (
        !prompt_.active ||
        !prompt_.pung
    )
    {
        return false;
    }


    prompt_ =
        ClaimPrompt();


    if (
        !doPung(
            EAST
        )
    )
    {
        return false;
    }


    Player& human =
        players_[0];


    // IMPORTANT:
    // If PUNG completes 4 sets + pair:
    // MAHJONG immediately - NO DISCARD.
    if (
        canMahjong(
            human
        )
    )
    {
        declareMahjong(
            EAST,
            "PUNG CLAIM"
        );

        return true;
    }


    awaitingHumanDiscard_ =
        true;


    message_ =
        "PUNG claimed - discard one tile.";


    return true;
}


// =====================================================
// HUMAN KONG
// =====================================================

bool GameCore::humanClaimKong()
{
    if (
        !prompt_.active ||
        !prompt_.kong
    )
    {
        return false;
    }


    prompt_ =
        ClaimPrompt();


    awaitingHumanDiscard_ =
        false;


    return doKongFromDiscard(
        EAST
    );
}


// =====================================================
// HUMAN MAHJONG FROM DISCARD
// =====================================================

bool GameCore::humanClaimMahjong()
{
    if (
        !prompt_.active ||
        !prompt_.mahjong
    )
    {
        return false;
    }


    std::string tile =
        lastDiscard_;


    Player& human =
        players_[0];


    human.hand.push_back(
        tile
    );


    sortHand(
        human
    );


    consumeLastDiscard();


    declareMahjong(
        EAST,
        "DISCARD CLAIM"
    );


    return true;
}


// =====================================================
// HUMAN SELF KONG
// =====================================================

bool GameCore::humanSelfKong(
    int handIndex
)
{
    if (
        handOver_ ||
        prompt_.active ||
        !awaitingHumanDiscard_ ||
        turn_ != EAST
    )
    {
        return false;
    }


    Player& human =
        players_[0];


    if (
        handIndex < 0 ||
        handIndex >=
            static_cast<int>(
                human.hand.size()
            )
    )
    {
        return false;
    }


    std::string tile =
        human.hand[
            handIndex
        ];


    if (
        countTile(
            human.hand,
            tile
        ) < 4
    )
    {
        message_ =
            "Selected tile cannot form KONG.";

        return false;
    }


    return doSelfKong(
        EAST,
        tile
    );
}


// =====================================================
// CHOW SELECTION
// =====================================================

void GameCore::selectNextChow(
    int direction
)
{
    if (
        !prompt_.active ||
        prompt_.chows.empty()
    )
    {
        return;
    }


    int count =
        static_cast<int>(
            prompt_.chows.size()
        );


    prompt_.selectedChow +=
        direction;


    while (
        prompt_.selectedChow < 0
    )
    {
        prompt_.selectedChow +=
            count;
    }


    while (
        prompt_.selectedChow >= count
    )
    {
        prompt_.selectedChow -=
            count;
    }
}


// =====================================================
// BOT DISCARD
// =====================================================

bool GameCore::botDiscard(
    Seat seat
)
{
    Player& bot =
        players_[
            seatIndex(seat)
        ];


    std::vector<int>
        legal;


    for (int i = 0;
         i <
            static_cast<int>(
                bot.hand.size()
            );
         ++i)
    {
        if (
            !bot.claimDiscardLock.empty() &&
            bot.hand[i] ==
                bot.claimDiscardLock
        )
        {
            continue;
        }


        legal.push_back(i);
    }


    if (legal.empty())
        return false;


    int selection =
        legal[
            nextRandom()
            %
            legal.size()
        ];


    return makeDiscard(
        seat,
        selection
    );
}


// =====================================================
// BOT NORMAL TURN
// =====================================================

void GameCore::botNormalTurn(
    Seat seat
)
{
    if (
        !drawOne(
            seat,
            true
        )
    )
    {
        return;
    }


    Player& bot =
        players_[
            seatIndex(seat)
        ];


    if (
        canSelfDrawMahjong(
            bot
        )
    )
    {
        declareMahjong(
            seat,
            "SELF DRAW"
        );

        return;
    }


    std::vector<std::string>
        kongs =
            selfKongs(bot);


    if (!kongs.empty())
    {
        doSelfKong(
            seat,
            kongs[0]
        );

        return;
    }


    botDiscard(
        seat
    );
}


// =====================================================
// RESOLVE DISCARD
// =====================================================

void GameCore::resolveDiscard()
{
    if (
        !resolvePending_ ||
        lastDiscard_.empty()
    )
    {
        resolvePending_ =
            false;

        return;
    }


    std::vector<ClaimOffer>
        offers =
            findClaims();


    if (offers.empty())
    {
        resolvePending_ =
            false;

        turn_ =
            nextSeat(
                lastDiscarder_
            );

        return;
    }


    // =================================================
    // HUMAN CLAIM WINDOW
    // =================================================

    for (size_t i = 0;
         i < offers.size();
         ++i)
    {
        if (
            offers[i].claimant ==
            EAST
        )
        {
            const ClaimOptions& options =
                offers[i].options;


            prompt_ =
                ClaimPrompt();


            prompt_.active =
                true;

            prompt_.from =
                lastDiscarder_;

            prompt_.tile =
                lastDiscard_;

            prompt_.chows =
                options.chows;

            prompt_.pung =
                options.pung;

            prompt_.kong =
                options.kong;

            prompt_.mahjong =
                options.mahjong;


            message_ =
                "Claim available.";


            return;
        }
    }


    // =================================================
    // BOT FIRST-COME CLAIM
    // =================================================

    int chosen =
        static_cast<int>(
            nextRandom()
            %
            offers.size()
        );


    ClaimOffer offer =
        offers[chosen];


    Seat bot =
        offer.claimant;


    // Mahjong from ANY opponent.
    if (
        offer.options.mahjong
    )
    {
        Player& player =
            players_[
                seatIndex(bot)
            ];


        player.hand.push_back(
            lastDiscard_
        );


        sortHand(
            player
        );


        consumeLastDiscard();


        declareMahjong(
            bot,
            "DISCARD CLAIM"
        );


        return;
    }


    // KONG first.
    if (
        offer.options.kong
    )
    {
        doKongFromDiscard(
            bot
        );

        return;
    }


    // PUNG.
    if (
        offer.options.pung
    )
    {
        if (
            !doPung(bot)
        )
        {
            return;
        }


        if (
            canMahjong(
                players_[
                    seatIndex(bot)
                ]
            )
        )
        {
            declareMahjong(
                bot,
                "PUNG CLAIM"
            );

            return;
        }


        // Leave the claimed meld visible and defer this bot's discard.
        botClaimDiscardPending_ = true;
        waitMs_ = BOT_THINK_MS;
        freshDelay_ = true;


        return;
    }


    // CHOW.
    if (
        !offer.options.chows.empty()
    )
    {
        if (
            !doChow(
                bot,
                offer.options.chows[0]
            )
        )
        {
            return;
        }


        if (
            canMahjong(
                players_[
                    seatIndex(bot)
                ]
            )
        )
        {
            declareMahjong(
                bot,
                "CHOW CLAIM"
            );

            return;
        }


        // Leave the claimed meld visible and defer this bot's discard.
        botClaimDiscardPending_ = true;
        waitMs_ = BOT_THINK_MS;
        freshDelay_ = true;


        return;
    }
}


// =====================================================
// GAME FLOW
// =====================================================

void GameCore::advance()
{
    if (!handOver_ && !awaitingHumanDiscard_ && !prompt_.active)
    {
        // Resolve previous discard.
        if (resolvePending_)
        {
            resolveDiscard();
            if (!handOver_ && !prompt_.active && !resolvePending_ && turn_ != EAST)
            {
                waitMs_ = BOT_THINK_MS;
                freshDelay_ = true;
                message_ = seatName(turn_) + " thinking...";
            }
            return;
        }


        // Human normal turn.
        if (turn_ == EAST)
        {
            if (
                !drawOne(
                    EAST,
                    true
                )
            )
            {
                return;
            }


            Player& human =
                players_[0];


            if (
                canSelfDrawMahjong(
                    human
                )
            )
            {
                declareMahjong(
                    EAST,
                    "SELF DRAW"
                );

                return;
            }


            awaitingHumanDiscard_ =
                true;


            message_ =
                "Your turn - choose a discard.";


            return;
        }


        // Bot normal turn.
        // After a CHOW/PUNG the bot owes a discard, not another wall draw.
        if (botClaimDiscardPending_)
        {
            botClaimDiscardPending_ = false;
            botDiscard(turn_);
        }
        else
            botNormalTurn(turn_);

        if (!handOver_ && !resolvePending_)
        {
            waitMs_ = BOT_THINK_MS;
            freshDelay_ = true;
        }
    }
}

void GameCore::update(unsigned int elapsedMs)
{
    if (handOver_ || awaitingHumanDiscard_ || prompt_.active)
        return;
    if (freshDelay_)
    {
        // Never charge time from before the action to its new display pause.
        // In particular, an input-generated discard must get a rendered frame.
        freshDelay_ = false;
        return;
    }
    if (elapsedMs < waitMs_)
    {
        waitMs_ -= elapsedMs;
        return;
    }
    waitMs_ = 0;
    // Deliberately discard any excess elapsed time: a slow frame must never
    // skip several bot discards before the renderer gets to show them.
    advance();
}

int GameCore::highlightedDiscard() const
{
    return highlightedDiscard_;
}


// =====================================================
// PUBLIC GETTERS
// =====================================================

const Player& GameCore::player(
    int index
) const
{
    return players_[index];
}


const std::vector<DiscardEntry>&
GameCore::river() const
{
    return river_;
}


const ClaimPrompt&
GameCore::claimPrompt() const
{
    return prompt_;
}


int GameCore::wallCount() const
{
    return static_cast<int>(
        wall_.size()
    );
}


Seat GameCore::turn() const
{
    return turn_;
}


bool GameCore::humanNeedsDiscard() const
{
    return awaitingHumanDiscard_;
}


bool GameCore::handOver() const
{
    return handOver_;
}


bool GameCore::hasWinner() const
{
    return hasWinner_;
}


Seat GameCore::winner() const
{
    return winner_;
}


const std::string&
GameCore::winningMethod() const
{
    return winningMethod_;
}


const std::string&
GameCore::message() const
{
    return message_;
}
