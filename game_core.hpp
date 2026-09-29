#ifndef DEERS_MAHJONG_GAME_CORE_HPP
#define DEERS_MAHJONG_GAME_CORE_HPP

#include <stdint.h>
#include <string>
#include <vector>

enum Seat
{
    EAST = 0,
    SOUTH = 1,
    WEST = 2,
    NORTH = 3
};

enum ClaimPermission
{
    CLAIM_NONE,
    CLAIM_CHOW,
    CLAIM_PUNG
};

struct Meld
{
    std::string kind;
    std::vector<std::string> physical;
    std::vector<std::string> logical;
};

struct Player
{
    Seat seat;
    std::string name;

    std::vector<std::string> hand;
    std::vector<Meld> melds;

    bool mustDiscard;
    bool mrh;

    std::string justDrew;
    std::string claimDiscardLock;
};

struct DiscardEntry
{
    Seat seat;
    std::string tile;
};

struct ClaimOptions
{
    std::vector<std::vector<std::string> > chows;

    bool pung;
    bool kong;
    bool mahjong;

    ClaimOptions()
        : pung(false),
          kong(false),
          mahjong(false)
    {
    }
};

struct ClaimOffer
{
    Seat claimant;
    ClaimPermission permission;
    ClaimOptions options;
};

struct ClaimPrompt
{
    bool active;

    Seat from;
    std::string tile;

    std::vector<std::vector<std::string> > chows;

    bool pung;
    bool kong;
    bool mahjong;

    int selectedChow;

    ClaimPrompt()
        : active(false),
          from(EAST),
          pung(false),
          kong(false),
          mahjong(false),
          selectedChow(0)
    {
    }
};


class GameCore
{
public:

    GameCore();

    void newHand(uint32_t seed);

    // Execute at most one automatic transition. update() adds presentation
    // timing without blocking rendering, input or audio.
    void advance();
    void update(unsigned int elapsedMs);
    int highlightedDiscard() const;


    // =================================================
    // HUMAN ACTIONS
    // =================================================

    bool humanDiscard(int index);

    bool humanCanDeclareMrh() const;
    bool humanDeclareMrh();
    int humanMrhAdjustment() const;

    bool humanPass();

    bool humanClaimChow();

    bool humanClaimPung();

    bool humanClaimKong();

    bool humanClaimMahjong();

    bool humanSelfKong(int handIndex);

    void selectNextChow(int direction);


    // =================================================
    // STATE ACCESS
    // =================================================

    const Player& player(int index) const;

    const std::vector<DiscardEntry>& river() const;

    const ClaimPrompt& claimPrompt() const;

    int wallCount() const;

    Seat turn() const;

    bool humanNeedsDiscard() const;

    bool handOver() const;

    bool hasWinner() const;

    Seat winner() const;

    const std::string& winningMethod() const;

    const std::string& message() const;


private:

    Player players_[4];

    std::vector<std::string> wall_;
    std::vector<DiscardEntry> river_;

    Seat turn_;

    std::string lastDiscard_;
    Seat lastDiscarder_;

    bool awaitingHumanDiscard_;
    bool resolvePending_;

    bool handOver_;
    bool hasWinner_;

    Seat winner_;

    std::string winningMethod_;
    std::string message_;

    ClaimPrompt prompt_;

    uint32_t rngState_;
    unsigned int waitMs_;
    bool freshDelay_;
    bool botClaimDiscardPending_;
    int highlightedDiscard_;


    // =================================================
    // RNG / WALL
    // =================================================

    uint32_t nextRandom();

    void buildWall();

    void shuffleWall();

    bool drawOne(
        Seat seat,
        bool announce
    );


    // =================================================
    // HELPERS
    // =================================================

    int seatIndex(Seat seat) const;

    Seat nextSeat(Seat seat) const;

    std::string seatName(Seat seat) const;

    ClaimPermission claimPermission(
        Seat discarder,
        Seat claimant
    ) const;

    bool isSuited(
        const std::string& tile
    ) const;

    int tileRank(
        const std::string& tile
    ) const;

    void sortHand(
        Player& player
    );

    int countTile(
        const std::vector<std::string>& hand,
        const std::string& tile
    ) const;

    bool removeOne(
        std::vector<std::string>& hand,
        const std::string& tile
    ) const;


    // =================================================
    // CLAIM LOGIC
    // =================================================

    std::vector<std::vector<std::string> >
    possibleChows(
        const Player& player,
        const std::string& discard
    ) const;

    bool canPung(
        const Player& player,
        const std::string& discard
    ) const;

    bool canKong(
        const Player& player,
        const std::string& discard
    ) const;

    ClaimOptions inspectClaim(
        Seat claimant
    ) const;

    std::vector<ClaimOffer>
    findClaims() const;


    // =================================================
    // WIN SOLVER
    // =================================================

    bool buildSetsRecursive(
        std::vector<std::string> tiles,
        int setsNeeded
    ) const;

    bool canMahjong(
        const Player& player
    ) const;

    bool canMahjongWithExtra(
        const Player& player,
        const std::string& extra
    ) const;

    bool canSelfDrawMahjong(
        const Player& player
    ) const;

    void declareMahjong(
        Seat seat,
        const std::string& method
    );


    // =================================================
    // DISCARD / MELD EXECUTION
    // =================================================

    bool makeDiscard(
        Seat seat,
        int index
    );

    void consumeLastDiscard();

    bool doChow(
        Seat claimant,
        const std::vector<std::string>& sequence
    );

    bool doPung(
        Seat claimant
    );

    bool doKongFromDiscard(
        Seat claimant
    );

    bool doSelfKong(
        Seat seat,
        const std::string& tile
    );


    // =================================================
    // BOT / FLOW
    // =================================================

    void resolveDiscard();

    void botNormalTurn(
        Seat seat
    );

    bool botDiscard(
        Seat seat
    );

    std::vector<std::string>
    selfKongs(
        const Player& player
    ) const;
};

#endif
