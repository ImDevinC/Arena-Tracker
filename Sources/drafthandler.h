#ifndef DRAFTHANDLER_H
#define DRAFTHANDLER_H

#include "Widgets/ui_extended.h"
#include "Cards/draftcard.h"
#include "utility.h"
#include "Widgets/draftscorewindow.h"
#include "Widgets/draftitemcounter.h"
#include <QObject>
#include <QFutureWatcher>


#define CAPTUREDRAFT_START_TIME         1500
#define CAPTUREDRAFT_LOOP_TIME          100
#define CAPTUREDRAFT_LOOP_FLANN_TIME    200

#define CARD_ACCEPTED_THRESHOLD             0.35
#define CARD_ACCEPTED_THRESHOLD_INCREASE    0.02


enum VisibleRace {V_MURLOC, V_DEMON, V_MECHANICAL, V_ELEMENTAL, V_BEAST, V_TOTEM, V_PIRATE, V_DRAGON, V_NUM_RACES};
enum VisibleType {V_MINION, V_SPELL, V_WEAPON, V_NUM_TYPES};
enum VisibleMechanics {V_DISCOVER_DRAW, V_TAUNT, /*V_RESTORE,*/
                       V_AOE, V_PING, V_DAMAGE_DESTROY, V_REACH,
                       V_ENRAGED,
                       /*V_BATTLECRY, V_COMBO, V_DEATHRATTLE, V_DIVINE_SHIELD,
                       V_FREEZE, V_OVERLOAD, V_SECRET, V_STEALTH, */V_NUM_MECHANICS};


class LFtier
{
public:
    int score = 0;
    int maxCard = -1;
};

class ScreenDetection
{
public:
    cv::Rect screenRects[3];
    int screenIndex = -1;
    QPointF screenScale = QPointF(0,0);
};

class DraftHandler : public QObject
{
    Q_OBJECT
public:
    DraftHandler(QObject *parent, Ui::Extended *ui);
    ~DraftHandler();

//Variables
private:
    Ui::Extended *ui;
    QMap<QString, QList<QString>> synergyCodes;
    QMap<QString, int> hearthArenaTiers;
    QMap<QString, LFtier> lightForgeTiers;
    QMap<QString, cv::MatND> cardsHist;
    QStringList cardsDownloading;
    DraftCard draftCards[3];
    //Guarda los mejores candidatos de esta iteracion
    QMap<QString, DraftCard> draftCardMaps[3];  //[Code(_premium)] --> DraftCard
    //Se crea al final de la iteracion para ordenar los candidatos por match score
    QMap<double, QString> bestMatchesMaps[3];   //[Match] --> Code(_premium)
    bool cardDetected[3];
    QString arenaHero;
    double deckRating;
    cv::Rect screenRects[3];
    int screenIndex;
    int numCaptured;
    bool drafting, capturing, leavingArena;
    bool mouseInApp;
    Transparency transparency;
    DraftScoreWindow *draftScoreWindow;
    bool showDraftOverlay;
    bool learningMode;
    QString justPickedCard; //Evita doble pick card en Arena.log
    DraftMethod draftMethod;
    QFutureWatcher<ScreenDetection> futureFindScreenRects;
    QLabel *labelCard[3];
    QLabel *labelLFscore[3];
    QLabel *labelHAscore[3];
    double shownTierScores[3];
    DraftItemCounter **raceCounters, **cardTypeCounters, **mechanicCounters;
    QHBoxLayout *horLayoutRaces1, *horLayoutRaces2, *horLayoutCardTypes;
    QHBoxLayout *horLayoutMechanics1, *horLayoutMechanics2;


//Metodos
private:
    void completeUI();
    cv::MatND getHist(const QString &code);
    cv::MatND getHist(cv::Mat &srcBase);
    void initCodesAndHistMaps(QString &hero);
    void resetTab(bool alreadyDrafting);
    void clearLists(bool keepCounters);
    bool getScreenCardsHist(cv::MatND screenCardsHist[3]);
    void showNewCards(DraftCard bestCards[]);
    void updateBoxTitle(double cardRating=0);
    bool screenFound();
    ScreenDetection findScreenRects();
    void clearScore(QLabel *label, DraftMethod draftMethod, bool clearText=true);
    void highlightScore(QLabel *label, DraftMethod draftMethod);
    void deleteDraftScoreWindow();
    void showOverlay();
    void newCaptureDraftLoop(bool delayed=false);
    void updateScoresVisibility();
    void initHearthArenaTiers(const QString &heroString);
    QMap<QString, LFtier> initLightForgeTiers(const QString &heroString);
    void createDraftScoreWindow(const QPointF &screenScale);
    void initCounters(QList<DeckCard> deckCardList);
    void mapBestMatchingCodes(cv::MatND screenCardsHist[]);
    double getMinMatch(const QMap<QString, DraftCard> &draftCardMaps);
    bool areCardsDetected();
    void buildBestMatchesMaps();
    void getBestCards(DraftCard bestCards[3]);
    void addCardHist(QString code, bool premium);
    QString degoldCode(QString fileName);
    int normalizeLFscore(int score);
    void createDraftItemCounters();
    void deleteDraftItemCounters();
    void updateRaceCounters(DeckCard &deckCard);
    void updateCardTypeCounters(DeckCard &deckCard);
    void getSynergies(DraftCard &draftCard, QMap<QString, int> &synergies);
    void getCardTypeSynergies(DraftCard &draftCard, QMap<QString, int> &synergies);
    void getRaceSynergies(DraftCard &draftCard, QMap<QString, int> &synergies);
    void getMechanicSynergies(DraftCard &draftCard, QMap<QString, int> &synergies);
    void initSynergyCodes();
    int draftedCardsCount();
    void updateCounters(DeckCard &deckCard);
    void updateMechanicCounters(DeckCard &deckCard);

//public:
    bool isSpellGen(const QString &code);
    bool isWeaponGen(const QString &code);
    bool isMurlocGen(const QString &code);
    bool isDemonGen(const QString &code);
    bool isMechGen(const QString &code);
    bool isElementalGen(const QString &code);
    bool isBeastGen(const QString &code);
    bool isTotemGen(const QString &code);
    bool isPirateGen(const QString &code);
    bool isDragonGen(const QString &code);
    bool isDiscoverDrawGen(const QString &code);
    bool isTauntGen(const QString &code, const QJsonArray &mechanics, const QJsonArray &referencedTags);
    bool isAoeGen(const QString &code);
    bool isDamageMinionsGen(const QString &code, const QJsonArray &mechanics, const QJsonArray &referencedTags, const QString &text, const CardType &cardType, int attack);
    bool isDestroyGen(const QString &code);
    bool isPingGen(const QString &code, const QJsonArray &mechanics, const QJsonArray &referencedTags,
                   const QString &text, const CardType &cardType, int attack);
    bool isReachGen(const QString &code, const QJsonArray &mechanics, const QJsonArray &referencedTags, const QString &text, const CardType &cardType);
    bool isEnrageGen(const QString &code, const QJsonArray &mechanics, const QJsonArray &referencedTags);

    bool isMurlocSyn(const QString &code);
    bool isDemonSyn(const QString &code);
    bool isMechSyn(const QString &code);
    bool isElementalSyn(const QString &code);
    bool isBeastSyn(const QString &code);
    bool isTotemSyn(const QString &code);
    bool isPirateSyn(const QString &code);
    bool isDragonSyn(const QString &code);
    bool isSpellSyn(const QString &code);
    bool isWeaponSyn(const QString &code);
    bool isEnrageSyn(const QString &code, const QString &text);

public:
    void reHistDownloadedCardImage(const QString &fileNameCode, bool missingOnWeb=false);
    void setMouseInApp(bool value);
    void setTransparency(Transparency value);
    void setShowDraftOverlay(bool value);
    void setLearningMode(bool value);
    void redrawAllCards();
    void updateTamCard(int value);
    void setDraftMethod(DraftMethod value);
    void setTheme();
    void craftGoldenCopy(int cardIndex);
    bool isDrafting();
    void deMinimizeScoreWindow();
    QStringList getAllArenaCodes();

signals:
    void checkCardImage(QString code);
    void newDeckCard(QString code);
    void draftStarted();
    void draftEnded();
    void downloadStarted();
    void downloadEnded();
    void overlayCardEntered(QString code, QRect rectCard, int maxTop, int maxBottom, bool alignReverse=true);
    void overlayCardLeave();
    void advanceProgressBar(int remaining, QString text);
    void startProgressBar(int maximum, QString text);
    void showMessageProgressBar(QString text, int hideDelay = 5000);
    void pLog(QString line);
    void pDebug(QString line, DebugLevel debugLevel=Normal, QString file="DraftHandler");

public slots:
    void beginDraft(QString hero, QList<DeckCard> deckCardList=QList<DeckCard>());
    void endDraft();
    void showNewRatings(double rating1, double rating2, double rating3,
                        double tierScore1, double tierScore2, double tierScore3,
                        int maxCard1, int maxCard2, int maxCard3, DraftMethod draftMethod);
    void pickCard(QString code);
    void enterArena();
    void leaveArena();
    void minimizeScoreWindow();

private slots:
    void captureDraft();
    void finishFindScreenRects();
    void startFindScreenRects();
};

#endif // DRAFTHANDLER_H
