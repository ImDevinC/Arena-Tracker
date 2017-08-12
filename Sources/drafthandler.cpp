#include "drafthandler.h"
#include "mainwindow.h"
#include "themehandler.h"
#include <QtConcurrent/QtConcurrent>
#include <QtWidgets>

DraftHandler::DraftHandler(QObject *parent, Ui::Extended *ui) : QObject(parent)
{
    this->ui = ui;
    this->deckRating = 0;
    this->numCaptured = 0;
    this->drafting = false;
    this->capturing = false;
    this->leavingArena = false;
    this->transparency = Opaque;
    this->draftScoreWindow = NULL;
    this->synergyHandler = NULL;
    this->mouseInApp = false;
    this->draftMethod = All;

    for(int i=0; i<3; i++)
    {
        screenRects[i] = cv::Rect(0,0,0,0);
        cardDetected[i] = false;
    }

    createSynergyHandler();
    completeUI();

    connect(&futureFindScreenRects, SIGNAL(finished()), this, SLOT(finishFindScreenRects()));
}

DraftHandler::~DraftHandler()
{
    deleteDraftScoreWindow();
    if(synergyHandler != NULL)  delete synergyHandler;
}


void DraftHandler::createSynergyHandler()
{
    this->synergyHandler = new SynergyHandler(this->parent(),ui);
    connect(synergyHandler, SIGNAL(pLog(QString)),
            this, SIGNAL(pLog(QString)));
    connect(synergyHandler, SIGNAL(pDebug(QString,DebugLevel,QString)),
            this, SIGNAL(pDebug(QString,DebugLevel,QString)));
}


void DraftHandler::completeUI()
{
    labelCard[0] = ui->labelCard1;
    labelCard[1] = ui->labelCard2;
    labelCard[2] = ui->labelCard3;
    labelLFscore[0] = ui->labelLFscore1;
    labelLFscore[1] = ui->labelLFscore2;
    labelLFscore[2] = ui->labelLFscore3;
    labelHAscore[0] = ui->labelHAscore1;
    labelHAscore[1] = ui->labelHAscore2;
    labelHAscore[2] = ui->labelHAscore3;
}


QStringList DraftHandler::getAllArenaCodes()
{
    QStringList codeList;

    QFile jsonFile(Utility::extraPath() + "/lightForge.json");
    jsonFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonFile.readAll());
    jsonFile.close();
    const QJsonArray jsonCardsArray = jsonDoc.object().value("Cards").toArray();
    for(QJsonValue jsonCard: jsonCardsArray)
    {
        QJsonObject jsonCardObject = jsonCard.toObject();
        QString code = jsonCardObject.value("CardId").toString();
        codeList.append(code);
    }

    return codeList;
}


void DraftHandler::initHearthArenaTiers(const QString &heroString)
{
    hearthArenaTiers.clear();

    QFile jsonFile(Utility::extraPath() + "/hearthArena.json");
    jsonFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonFile.readAll());
    jsonFile.close();
    QJsonObject jsonNamesObject = jsonDoc.object().value(heroString).toObject();

    for(const QString &code: lightForgeTiers.keys())
    {
        QString name = Utility::cardEnNameFromCode(code);
        int score = jsonNamesObject.value(name).toInt();
        hearthArenaTiers[code] = score;
        if(score == 0)  emit pDebug("HearthArena missing: " + name);
    }

    emit pDebug("HearthArena Cards: " + QString::number(hearthArenaTiers.count()));
}


void DraftHandler::addCardHist(QString code, bool premium)
{
    QString fileNameCode = premium?(code + "_premium"): code;
    QFileInfo cardFile(Utility::hscardsPath() + "/" + fileNameCode + ".png");
    if(cardFile.exists())
    {
        cardsHist[fileNameCode] = getHist(fileNameCode);
    }
    else
    {
        //La bajamos de HearthHead
        emit checkCardImage(fileNameCode);
        cardsDownloading.append(fileNameCode);
    }
}


QMap<QString, LFtier> DraftHandler::initLightForgeTiers(const QString &heroString)
{
    QMap<QString, LFtier> lightForgeTiers;

    QFile jsonFile(Utility::extraPath() + "/lightForge.json");
    jsonFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonFile.readAll());
    jsonFile.close();
    const QJsonArray jsonCardsArray = jsonDoc.object().value("Cards").toArray();
    for(QJsonValue jsonCard: jsonCardsArray)
    {
        QJsonObject jsonCardObject = jsonCard.toObject();
        QString code = jsonCardObject.value("CardId").toString();

        const QJsonArray jsonScoresArray = jsonCardObject.value("Scores").toArray();
        for(QJsonValue jsonScore: jsonScoresArray)
        {
            QJsonObject jsonScoreObject = jsonScore.toObject();
            QString hero = jsonScoreObject.value("Hero").toString();

            if(hero == NULL || hero == heroString)
            {
                LFtier lfTier;
                lfTier.score = (int)jsonScoreObject.value("Score").toDouble();

                if(jsonScoreObject.value("StopAfterFirst").toBool())
                {
                    lfTier.maxCard = 1;
                }
                else if(jsonScoreObject.value("StopAfterSecond").toBool())
                {
                    lfTier.maxCard = 2;
                }
                else
                {
                    lfTier.maxCard = -1;
                }

                if(!lightForgeTiers.contains(code))
                {
                    addCardHist(code, false);
                    addCardHist(code, true);
                }
                lightForgeTiers[code] = lfTier;
            }
        }
    }

    emit pDebug("LightForge Cards: " + QString::number(lightForgeTiers.count()));
    return lightForgeTiers;
}


void DraftHandler::initCodesAndHistMaps(QString &hero)
{
    cardsDownloading.clear();
    cardsHist.clear();

    startFindScreenRects();
    const QString heroString = Utility::heroString2FromLogNumber(hero);
    this->lightForgeTiers = initLightForgeTiers(heroString);
    initHearthArenaTiers(heroString);
    synergyHandler->initSynergyCodes();

    //Wait for cards
    if(cardsDownloading.isEmpty())
    {
        newCaptureDraftLoop();
    }
    else
    {
        emit startProgressBar(cardsDownloading.count(), "Downloading cards...");
        emit downloadStarted();
    }
}


void DraftHandler::reHistDownloadedCardImage(const QString &fileNameCode, bool missingOnWeb)
{
    if(!cardsDownloading.contains(fileNameCode)) return; //No forma parte del drafting

    if(!fileNameCode.isEmpty() && !missingOnWeb)  cardsHist[fileNameCode] = getHist(fileNameCode);
    cardsDownloading.removeOne(fileNameCode);
    emit advanceProgressBar(cardsDownloading.count(), fileNameCode.split("_premium").first() + " downloaded");
    if(cardsDownloading.isEmpty())
    {
        emit showMessageProgressBar("All cards downloaded");
        emit downloadEnded();
        newCaptureDraftLoop();
    }
}


void DraftHandler::resetTab(bool alreadyDrafting)
{
    for(int i=0; i<3; i++)
    {
        clearScore(labelLFscore[i], LightForge);
        clearScore(labelHAscore[i], HearthArena);
        draftCards[i].setCode("");
        draftCards[i].draw(labelCard[i]);
    }

    updateBoxTitle();

    if(!alreadyDrafting)
    {
        //SizePreDraft
        MainWindow *mainWindow = ((MainWindow*)parent());
        QSettings settings("Arena Tracker", "Arena Tracker");
        settings.setValue("size", mainWindow->size());

        //Show Tab
        ui->tabWidget->insertTab(0, ui->tabDraft, QIcon(ThemeHandler::tabArenaFile()), "");
        ui->tabWidget->setTabToolTip(0, "Draft");

        //SizeDraft
        QSize sizeDraft = settings.value("sizeDraft", QSize(350, 400)).toSize();
        mainWindow->resize(sizeDraft);
        mainWindow->resizeTabWidgets();
    }

    ui->tabWidget->setCurrentWidget(ui->tabDraft);
}


void DraftHandler::clearLists(bool keepCounters)
{
    synergyHandler->clearLists(keepCounters);
    hearthArenaTiers.clear();
    lightForgeTiers.clear();
    cardsHist.clear();

    if(!keepCounters)
    {
        deckRating = 0;
    }

    for(int i=0; i<3; i++)
    {
        screenRects[i]=cv::Rect(0,0,0,0);
        cardDetected[i] = false;
        draftCardMaps[i].clear();
        bestMatchesMaps[i].clear();
    }

    screenIndex = -1;
    numCaptured = 0;
}


void DraftHandler::enterArena()
{
    if(drafting)
    {
        showOverlay();
        if(draftCards[0].getCode().isEmpty())
        {
            newCaptureDraftLoop(true);
        }
    }
}


void DraftHandler::leaveArena()
{
    if(drafting)
    {
        if(capturing)
        {
            this->leavingArena = true;
            this->numCaptured = 0;

            //Clear guessed cards
            for(int i=0; i<3; i++)
            {
                cardDetected[i] = false;
                draftCardMaps[i].clear();
                bestMatchesMaps[i].clear();
            }
        }
        if(draftScoreWindow != NULL)    draftScoreWindow->hide();
    }
}


void DraftHandler::beginDraft(QString hero, QList<DeckCard> deckCardList)
{
    bool alreadyDrafting = drafting;
    int heroInt = hero.toInt();
    if(heroInt<1 || heroInt>9)
    {
        emit pDebug("Begin draft of unknown hero: " + hero, Error);
        emit pLog(tr("Draft: ERROR: Started draft of unknown hero ") + hero);
        return;
    }
    else
    {
        emit pDebug("Begin draft. Heroe: " + hero);
        emit pLog(tr("Draft: New draft started."));
    }

    //Set updateTime in log / Hide card Window
    emit draftStarted();

    clearLists(true);

    this->arenaHero = hero;
    this->drafting = true;
    this->leavingArena = false;
    this->justPickedCard = "";

    initCodesAndHistMaps(hero);
    synergyHandler->initCounters(deckCardList);
    resetTab(alreadyDrafting);
}


void DraftHandler::endDraft()
{
    if(!drafting)    return;

    emit pLog(tr("Draft: ") + ui->groupBoxDraft->title());
    emit pDebug("End draft.");
    emit pLog(tr("Draft: Draft ended."));


    //SizeDraft
    MainWindow *mainWindow = ((MainWindow*)parent());
    QSettings settings("Arena Tracker", "Arena Tracker");
    settings.setValue("sizeDraft", mainWindow->size());

    //Hide Tab
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabDraft));
    ui->tabWidget->setCurrentIndex(ui->tabWidget->indexOf(ui->tabArena));
    mainWindow->calculateMinimumWidth();

    //SizePreDraft
    QSize size = settings.value("size", QSize(400, 400)).toSize();
    mainWindow->resize(size);

    //Upload or complete deck with assets
    //Set updateTime in log
    emit draftEnded();

    //Show Deck Score
    int numCards = synergyHandler->draftedCardsCount();
    int deckScore = (numCards==0)?0:(int)(deckRating/numCards);
    emit showMessageProgressBar("Deck Score: " + QString::number(deckScore), 10000);

    clearLists(false);

    this->drafting = false;
    this->justPickedCard = "";

    deleteDraftScoreWindow();

}


void DraftHandler::deleteDraftScoreWindow()
{
    if(draftScoreWindow != NULL)
    {
        draftScoreWindow->close();
        delete draftScoreWindow;
        draftScoreWindow = NULL;
    }
}


void DraftHandler::newCaptureDraftLoop(bool delayed)
{
    if(!capturing && drafting &&
        screenFound() && cardsDownloading.isEmpty() &&
        !lightForgeTiers.empty() && !hearthArenaTiers.empty())
    {
        capturing = true;

        if(delayed)                 QTimer::singleShot(CAPTUREDRAFT_START_TIME, this, SLOT(captureDraft()));
        else                        captureDraft();
    }
}


//Screen Rects detectados
void DraftHandler::captureDraft()
{
    justPickedCard = "";

    if(leavingArena || !drafting ||
        !screenFound() || !cardsDownloading.isEmpty() ||
        lightForgeTiers.empty() || hearthArenaTiers.empty())
    {
        leavingArena = false;
        capturing = false;
        return;
    }

    cv::MatND screenCardsHist[3];
    if(!getScreenCardsHist(screenCardsHist))
    {
        capturing = false;
        return;
    }
    mapBestMatchingCodes(screenCardsHist);

    if(areCardsDetected())
    {
        capturing = false;
        buildBestMatchesMaps();

        DraftCard bestCards[3];
        getBestCards(bestCards);
        showNewCards(bestCards);
    }
    else
    {
        QTimer::singleShot(CAPTUREDRAFT_LOOP_TIME, this, SLOT(captureDraft()));
    }
}


bool DraftHandler::areCardsDetected()
{
    for(int i=0; i<3; i++)
    {
        if(!cardDetected[i] && (numCaptured > 2) &&
            (getMinMatch(draftCardMaps[i]) < (CARD_ACCEPTED_THRESHOLD + numCaptured*CARD_ACCEPTED_THRESHOLD_INCREASE)))
        {
            cardDetected[i] = true;
        }
    }

    return (cardDetected[0] && cardDetected[1] && cardDetected[2]);
}


double DraftHandler::getMinMatch(const QMap<QString, DraftCard> &draftCardMaps)
{
    double minMatch = 1;
    for(DraftCard card: draftCardMaps.values())
    {
        double match = card.getSumQualityMatches();
        if(match < minMatch)    minMatch = match;
    }
    return minMatch;
}


void DraftHandler::buildBestMatchesMaps()
{
    for(int i=0; i<3; i++)
    {
        for(QString code: draftCardMaps[i].keys())
        {
            double match = draftCardMaps[i][code].getSumQualityMatches();
            bestMatchesMaps[i].insertMulti(match, code);
        }
    }
}


CardRarity DraftHandler::getBestRarity()
{
    CardRarity rarity[3];
    for(int i=0; i<3; i++)
    {
        rarity[i] = draftCardMaps[i][bestMatchesMaps[i].first()].getRarity();
    }

    if(rarity[0] == rarity[1] || rarity[0] == rarity[2])    return rarity[0];
    else if(rarity[1] == rarity[2])                         return rarity[1];
    else
    {
        double bestMatch = 1;
        int bestIndex = 0;

        for(int i=0; i<3; i++)
        {
            double match = bestMatchesMaps[i].firstKey();
            if(match < bestMatch)
            {
                bestMatch = match;
                bestIndex = i;
            }
        }

        return rarity[bestIndex];
    }
}


void DraftHandler::getBestCards(DraftCard bestCards[3])
{
    CardRarity bestRarity = getBestRarity();

    for(int i=0; i<3; i++)
    {
        QList<double> bestMatchesList = bestMatchesMaps[i].keys();
        QList<QString> bestCodesList = bestMatchesMaps[i].values();
        for(int j=0; j<bestMatchesList.count(); j++)
        {
            double match = bestMatchesList[j];
            QString code = bestCodesList[j];
            QString name = draftCardMaps[i][code].getName();
            QString cardInfo = code + " " + name + " " +
                    QString::number(((int)(match*1000))/1000.0);
            if(draftCardMaps[i][code].getRarity() == bestRarity)
            {
                bestCards[i] = draftCardMaps[i][code];
                emit pDebug("Choose: " + cardInfo);
                break;
            }
            else
            {
                emit pDebug("Skip: " + cardInfo + " (Different rarity)");
            }
        }
    }

    emit pDebug("(" + QString::number(synergyHandler->draftedCardsCount()) + ") " +
                bestCards[0].getCode() + "/" + bestCards[1].getCode() +
                "/" + bestCards[2].getCode() + " New codes.");
}


void DraftHandler::pickCard(QString code)
{
    if(!drafting || justPickedCard==code)
    {
        emit pDebug("WARNING: Duplicate pick code detected: " + code);
        return;
    }

    if(code=="0" || code=="1" || code=="2")
    {
        code = draftCards[code.toInt()].getCode();
    }

    DraftCard draftCard;
    for(int i=0; i<3; i++)
    {
        if(draftCards[i].getCode() == code)
        {
            draftCard = draftCards[i];
            synergyHandler->updateCounters(draftCard);
            updateBoxTitle(shownTierScores[i]);
            break;
        }
    }

    //Clear cards and score
    for(int i=0; i<3; i++)
    {
        clearScore(labelLFscore[i], LightForge);
        clearScore(labelHAscore[i], HearthArena);
        draftCards[i].setCode("");
        draftCards[i].draw(labelCard[i]);
        cardDetected[i] = false;
        draftCardMaps[i].clear();
        bestMatchesMaps[i].clear();
    }

    this->numCaptured = 0;
    if(draftScoreWindow != NULL)    draftScoreWindow->hideScores();

    emit pDebug("Pick card: " + code);
    emit pLog(tr("Draft:") + " (" + QString::number(synergyHandler->draftedCardsCount()) + ")" + draftCard.getName());
    emit newDeckCard(code);
    this->justPickedCard = code;

    newCaptureDraftLoop(true);
}


int DraftHandler::normalizeLFscore(int score)
{
    return score - 40;
}


void DraftHandler::showNewCards(DraftCard bestCards[3])
{
    //Load cards
    for(int i=0; i<3; i++)
    {
        clearScore(labelLFscore[i], LightForge);
        clearScore(labelHAscore[i], HearthArena);
        draftCards[i] = bestCards[i];
        draftCards[i].draw(labelCard[i]);
    }


    //LightForge
    int rating1 = normalizeLFscore(lightForgeTiers[bestCards[0].getCode()].score);
    int rating2 = normalizeLFscore(lightForgeTiers[bestCards[1].getCode()].score);
    int rating3 = normalizeLFscore(lightForgeTiers[bestCards[2].getCode()].score);
    int maxCard1 = lightForgeTiers[bestCards[0].getCode()].maxCard;
    int maxCard2 = lightForgeTiers[bestCards[1].getCode()].maxCard;
    int maxCard3 = lightForgeTiers[bestCards[2].getCode()].maxCard;
    showNewRatings(rating1, rating2, rating3,
                   rating1, rating2, rating3,
                   maxCard1, maxCard2, maxCard3,
                   LightForge);


    //HearthArena
    rating1 = hearthArenaTiers[bestCards[0].getCode()];
    rating2 = hearthArenaTiers[bestCards[1].getCode()];
    rating3 = hearthArenaTiers[bestCards[2].getCode()];
    showNewRatings(rating1, rating2, rating3,
                   rating1, rating2, rating3,
                   -1, -1, -1,
                   HearthArena);

    //Synergies //TODO Borrar para eliminar synergies
#ifdef QT_DEBUG
    QMap<QString,int> synergies[3];
    QStringList mechanicIcons[3];
    for(int i=0; i<3; i++)  synergyHandler->getSynergies(bestCards[i], synergies[i], mechanicIcons[i]);
    draftScoreWindow->setSynergies(synergies, mechanicIcons);
#endif
}


void DraftHandler::updateBoxTitle(double cardRating)
{
    deckRating += cardRating;
    int numCards = synergyHandler->draftedCardsCount();
    int actualRating = (numCards==0)?0:(int)(deckRating/numCards);
    ui->groupBoxDraft->setTitle(QString("Deck Score: " + QString::number(actualRating) +
                                        " (" + QString::number(numCards) + "/30)"));
}


void DraftHandler::showNewRatings(double rating1, double rating2, double rating3,
                                  double tierScore1, double tierScore2, double tierScore3,
                                  int maxCard1, int maxCard2, int maxCard3,
                                  DraftMethod draftMethod)
{
    double ratings[3] = {rating1,rating2,rating3};
    double tierScore[3] = {tierScore1, tierScore2, tierScore3};
    int maxCards[3] = {maxCard1, maxCard2, maxCard3};
    double maxRating = std::max(std::max(rating1,rating2),rating3);

    for(int i=0; i<3; i++)
    {
        //TierScore for deck average
        if(draftMethod == this->draftMethod || (this->draftMethod == All && draftMethod == HearthArena))
        {
            shownTierScores[i] = tierScore[i];
        }

        //Update score label
        if(draftMethod == LightForge)
        {
            labelLFscore[i]->setText(QString::number((int)ratings[i]) +
                                               (maxCards[i]!=-1?(" - MAX(" + QString::number(maxCards[i]) + ")"):""));
            if(maxRating == ratings[i])     highlightScore(labelLFscore[i], draftMethod);
        }
        else if(draftMethod == HearthArena)
        {
            labelHAscore[i]->setText(QString::number((int)ratings[i]) +
                                                (maxCards[i]!=-1?(" - MAX(" + QString::number(maxCards[i]) + ")"):""));
            if(maxRating == ratings[i])     highlightScore(labelHAscore[i], draftMethod);
        }
    }

    //Mostrar score
    if(draftScoreWindow != NULL)
    {
        draftScoreWindow->setScores(rating1, rating2, rating3, draftMethod);
    }
}


bool DraftHandler::getScreenCardsHist(cv::MatND screenCardsHist[3])
{
    QList<QScreen *> screens = QGuiApplication::screens();
    if(screenIndex >= screens.count() || screenIndex < 0)  return false;
    QScreen *screen = screens[screenIndex];
    if (!screen) return false;

    QRect rect = screen->geometry();
    QImage image = screen->grabWindow(0,rect.x(),rect.y(),rect.width(),rect.height()).toImage();
    cv::Mat mat(image.height(),image.width(),CV_8UC4,image.bits(), image.bytesPerLine());

    cv::Mat screenCapture = mat.clone();

    cv::Mat bigCards[3];
    bigCards[0] = screenCapture(screenRects[0]);
    bigCards[1] = screenCapture(screenRects[1]);
    bigCards[2] = screenCapture(screenRects[2]);


//#ifdef QT_DEBUG
//    cv::imshow("Card1", bigCards[0]);
//    cv::imshow("Card2", bigCards[1]);
//    cv::imshow("Card3", bigCards[2]);
//#endif

    for(int i=0; i<3; i++)  screenCardsHist[i] = getHist(bigCards[i]);
    return true;
}


QString DraftHandler::degoldCode(QString fileName)
{
    QString code = fileName;
    if(code.endsWith("_premium"))   code.chop(8);
    return code;
}


void DraftHandler::mapBestMatchingCodes(cv::MatND screenCardsHist[3])
{
    bool newCardsFound = false;

    for(int i=0; i<3; i++)
    {
        QMap<double, QString> bestMatchesMap;
        for(QMap<QString, cv::MatND>::const_iterator it=cardsHist.constBegin(); it!=cardsHist.constEnd(); it++)
        {
            double match = compareHist(screenCardsHist[i], it.value(), 3);
            QString code = it.key();
            bestMatchesMap.insertMulti(match, code);

            //Actualizamos DraftCardMaps con los nuevos resultados
            if(draftCardMaps[i].contains(code))
            {
                if(numCaptured != 0)    draftCardMaps[i][code].setBestQualityMatch(match);
            }
        }

        //Incluimos en DraftCardMaps los mejores 5 matches, si no han sido ya actualizados por estar en el map.
        QList<double> bestMatchesList = bestMatchesMap.keys();
        for(int j=0; j<5 && j<bestMatchesList.count(); j++)
        {
            double match = bestMatchesList.at(j);
            QString code = bestMatchesMap[match];

            if(!draftCardMaps[i].contains(code))
            {
                newCardsFound = true;
                draftCardMaps[i].insert(code, DraftCard(degoldCode(code)));
            }
        }
    }


    //No empezamos a contar mientras sigan apareciendo nuevas cartas en las 5 mejores posiciones
    if(!(numCaptured == 0 && newCardsFound))
    {
        this->numCaptured++;
    }


#ifdef QT_DEBUG
    for(int i=0; i<3; i++)
    {
        qDebug()<<endl;
        for(QString code: draftCardMaps[i].keys())
        {
            DraftCard card = draftCardMaps[i][code];
            qDebug()<<"["<<i<<"]"<<code<<card.getName()<<" -- "<<
                      ((int)(card.getSumQualityMatches()*1000))/1000.0;
        }
    }
    qDebug()<<"Captured: "<<numCaptured<<endl;
#endif
}


cv::MatND DraftHandler::getHist(const QString &code)
{
    cv::Mat fullCard = cv::imread((Utility::hscardsPath() + "/" + code + ".png").toStdString(), CV_LOAD_IMAGE_COLOR);
    cv::Mat srcBase;
    if(code.endsWith("_premium"))   srcBase = fullCard(cv::Rect(57,71,80,80));
    else                            srcBase = fullCard(cv::Rect(60,71,80,80));
    return getHist(srcBase);
}


cv::MatND DraftHandler::getHist(cv::Mat &srcBase)
{
    cv::Mat hsvBase;

    /// Convert to HSV
    cvtColor( srcBase, hsvBase, cv::COLOR_BGR2HSV );

    /// Using 50 bins for hue and 60 for saturation
    int h_bins = 50; int s_bins = 60;
    int histSize[] = { h_bins, s_bins };

    // hue varies from 0 to 179, saturation from 0 to 255
    float h_ranges[] = { 0, 180 };
    float s_ranges[] = { 0, 256 };
    const float* ranges[] = { h_ranges, s_ranges };

    // Use the o-th and 1-st channels
    int channels[] = { 0, 1 };

    /// Calculate the histograms for the HSV images
    cv::MatND histBase;
    calcHist( &hsvBase, 1, channels, cv::Mat(), histBase, 2, histSize, ranges, true, false );
    normalize( histBase, histBase, 0, 1, cv::NORM_MINMAX, -1, cv::Mat() );

    return histBase;
}


bool DraftHandler::screenFound()
{
    if(screenIndex != -1)   return true;
    else                    return false;
}


void DraftHandler::startFindScreenRects()
{
    if(!futureFindScreenRects.isRunning() && drafting)  futureFindScreenRects.setFuture(QtConcurrent::run(this, &DraftHandler::findScreenRects));
}


void DraftHandler::finishFindScreenRects()
{
    ScreenDetection screenDetection = futureFindScreenRects.result();

    if(screenDetection.screenIndex == -1)
    {
        this->screenIndex = -1;
        emit pDebug("Hearthstone arena screen not found. Retrying...");
        QTimer::singleShot(CAPTUREDRAFT_LOOP_FLANN_TIME, this, SLOT(startFindScreenRects()));
    }
    else
    {
        this->screenIndex = screenDetection.screenIndex;
        for(int i=0; i<3; i++)
        {
            this->screenRects[i] = screenDetection.screenRects[i];
        }

        emit pDebug("Hearthstone arena screen detected on screen " + QString::number(screenIndex));

        createDraftScoreWindow(screenDetection.screenScale);
        newCaptureDraftLoop();
    }
}


ScreenDetection DraftHandler::findScreenRects()
{
    ScreenDetection screenDetection;

    std::vector<Point2f> templatePoints(6);
    templatePoints[0] = cvPoint(205,276); templatePoints[1] = cvPoint(205+118,276+118);
    templatePoints[2] = cvPoint(484,276); templatePoints[3] = cvPoint(484+118,276+118);
    templatePoints[4] = cvPoint(762,276); templatePoints[5] = cvPoint(762+118,276+118);


    QList<QScreen *> screens = QGuiApplication::screens();
    for(int screenIndex=0; screenIndex<screens.count(); screenIndex++)
    {
        QScreen *screen = screens[screenIndex];
        if (!screen)    continue;

        std::vector<Point2f> screenPoints = Utility::findTemplateOnScreen("arenaTemplate.png", screen,
                                                                          templatePoints, screenDetection.screenScale);
        if(screenPoints.empty())    continue;

        //Calculamos screenRect
        for(int i=0; i<3; i++)
        {
            screenDetection.screenRects[i]=cv::Rect(screenPoints[i*2], screenPoints[i*2+1]);
        }

        screenDetection.screenIndex = screenIndex;
        return screenDetection;
    }

    screenDetection.screenIndex = -1;
    return screenDetection;
}


void DraftHandler::createDraftScoreWindow(const QPointF &screenScale)
{
    deleteDraftScoreWindow();
    QPoint topLeft(screenRects[0].x * screenScale.x(), screenRects[0].y * screenScale.y());
    QPoint bottomRight(screenRects[2].x * screenScale.x() + screenRects[2].width * screenScale.x(),
            screenRects[2].y * screenScale.y() + screenRects[2].height * screenScale.y());
    QRect draftRect(topLeft, bottomRight);
    QSize sizeCard(screenRects[0].width * screenScale.x(), screenRects[0].height * screenScale.y());
    draftScoreWindow = new DraftScoreWindow((QMainWindow *)this->parent(), draftRect, sizeCard, screenIndex);
    draftScoreWindow->setLearningMode(this->learningMode);
    draftScoreWindow->setDraftMethod(this->draftMethod);

    connect(draftScoreWindow, SIGNAL(cardEntered(QString,QRect,int,int)),
            this, SIGNAL(overlayCardEntered(QString,QRect,int,int)));
    connect(draftScoreWindow, SIGNAL(cardLeave()),
            this, SIGNAL(overlayCardLeave()));

    showOverlay();
}


void DraftHandler::clearScore(QLabel *label, DraftMethod draftMethod, bool clearText)
{
    if(clearText)   label->setText("");
    else if(label->styleSheet().contains("background-image"))
    {
        highlightScore(label, draftMethod);
        return;
    }

    if(!mouseInApp && transparency == Transparent)
    {
        label->setStyleSheet("QLabel {background-color: transparent; color: white;}");
    }
    else
    {
        label->setStyleSheet("");
    }
}


void DraftHandler::highlightScore(QLabel *label, DraftMethod draftMethod)
{
    QString backgroundImage = "";
    if(draftMethod == LightForge)           backgroundImage = ":/Images/bgScoreLF.png";
    else if(draftMethod == HearthArena)     backgroundImage = ":/Images/bgScoreHA.png";
    label->setStyleSheet("QLabel {background-color: transparent; color: " +
                         QString((!mouseInApp && transparency == Transparent)?"white":ThemeHandler::fgColor()) + ";"
                         "background-image: url(" + backgroundImage + "); background-repeat: no-repeat; background-position: center; }");
}


void DraftHandler::setTheme()
{
    QFont font(ThemeHandler::bigFont());
    font.setPixelSize(24);
    ui->labelLFscore1->setFont(font);
    ui->labelLFscore2->setFont(font);
    ui->labelLFscore3->setFont(font);
    ui->labelHAscore1->setFont(font);
    ui->labelHAscore2->setFont(font);
    ui->labelHAscore3->setFont(font);

    font = QFont(ThemeHandler::defaultFont());
    font.setPixelSize(12);

    for(int i=0; i<3; i++)
    {
        if(labelLFscore[i]->styleSheet().contains("background-image"))      highlightScore(labelLFscore[i], LightForge);
        if(labelHAscore[i]->styleSheet().contains("background-image"))      highlightScore(labelHAscore[i], HearthArena);
        draftCards[i].draw(labelCard[i]);
    }

    //Change Arena draft icon
    int index = ui->tabWidget->indexOf(ui->tabDraft);
    if(index >= 0)  ui->tabWidget->setTabIcon(index, QIcon(ThemeHandler::tabArenaFile()));
}


void DraftHandler::setTransparency(Transparency value)
{
    this->transparency = value;

    if(!mouseInApp && transparency==Transparent)
    {
        ui->tabDraft->setAttribute(Qt::WA_NoBackground);
        ui->tabDraft->repaint();

        ui->groupBoxDraft->setStyleSheet("QGroupBox{border: 0px solid transparent; margin-top: 15px; " + ThemeHandler::bgWidgets() +
                                            " color: white;}"
                                         "QGroupBox::title {subcontrol-origin: margin; subcontrol-position: top center;}");
    }
    else
    {
        ui->tabDraft->setAttribute(Qt::WA_NoBackground, false);
        ui->tabDraft->repaint();

        ui->groupBoxDraft->setStyleSheet("QGroupBox{border: 0px solid transparent; margin-top: 15px; " + ThemeHandler::bgWidgets() +
                                            " color: " + ThemeHandler::fgColor() + ";}"
                                         "QGroupBox::title {subcontrol-origin: margin; subcontrol-position: top center;}");
    }

    //Update score labels
    clearScore(ui->labelLFscore1, LightForge, false);
    clearScore(ui->labelLFscore2, LightForge, false);
    clearScore(ui->labelLFscore3, LightForge, false);
    clearScore(ui->labelHAscore1, HearthArena, false);
    clearScore(ui->labelHAscore2, HearthArena, false);
    clearScore(ui->labelHAscore3, HearthArena, false);

    //Update race counters
    synergyHandler->setTransparency(transparency, mouseInApp);
}


void DraftHandler::setMouseInApp(bool value)
{
    this->mouseInApp = value;
    setTransparency(this->transparency);
}


void DraftHandler::setShowDraftOverlay(bool value)
{
    this->showDraftOverlay = value;
    showOverlay();
}


void DraftHandler::showOverlay()
{
    if(this->draftScoreWindow != NULL)
    {
        if(this->showDraftOverlay)  this->draftScoreWindow->show();
        else                        this->draftScoreWindow->hide();
    }
}


void DraftHandler::setLearningMode(bool value)
{
    this->learningMode = value;
    if(this->draftScoreWindow != NULL)  draftScoreWindow->setLearningMode(value);

    updateScoresVisibility();
}


void DraftHandler::setDraftMethod(DraftMethod value)
{
    this->draftMethod = value;
    if(draftScoreWindow != NULL)    draftScoreWindow->setDraftMethod(value);

    updateScoresVisibility();
}


void DraftHandler::updateScoresVisibility()
{
    if(learningMode)
    {
        for(int i=0; i<3; i++)
        {
            labelLFscore[i]->hide();
            labelHAscore[i]->hide();
        }
    }
    else
    {
        switch(draftMethod)
        {
            case All:
                for(int i=0; i<3; i++)
                {
                    labelLFscore[i]->show();
                    labelHAscore[i]->show();
                }
                break;
            case LightForge:
                for(int i=0; i<3; i++)
                {
                    labelLFscore[i]->show();
                    labelHAscore[i]->hide();
                }
                break;
            case HearthArena:
                for(int i=0; i<3; i++)
                {
                    labelLFscore[i]->hide();
                    labelHAscore[i]->show();
                }
                break;
            default:
                for(int i=0; i<3; i++)
                {
                    labelLFscore[i]->hide();
                    labelHAscore[i]->hide();
                }
                break;
        }
    }
}


void DraftHandler::redrawAllCards()
{
    if(!drafting)   return;

    for(int i=0; i<3; i++)
    {
        draftCards[i].draw(labelCard[i]);
    }

    if(draftScoreWindow != NULL)    draftScoreWindow->redrawSynergyCards();
}


void DraftHandler::updateTamCard(int value)
{
    ui->labelCard1->setMaximumHeight(value);
    ui->labelCard2->setMaximumHeight(value);
    ui->labelCard3->setMaximumHeight(value);
}


void DraftHandler::craftGoldenCopy(int cardIndex)
{
    QString code = draftCards[cardIndex].getCode();
    if(!drafting || code.isEmpty())  return;

    //Lanza script
    QProcess p;
    QStringList params;

    params << QDir::toNativeSeparators(Utility::extraPath() + "/goldenCrafter.py");
    params << Utility::removeAccents(draftCards[cardIndex].getName());//Card Name

    emit pDebug("Start script:\n" + params.join(" - "));

#ifdef Q_OS_WIN
    p.start("python", params);
#else
    p.start("python3", params);
#endif
    p.waitForFinished(-1);
}


bool DraftHandler::isDrafting()
{
    return this->drafting;
}


void DraftHandler::minimizeScoreWindow()
{
    if(this->draftScoreWindow != NULL)  draftScoreWindow->showMinimized();
}


void DraftHandler::deMinimizeScoreWindow()
{
    if(this->draftScoreWindow != NULL)  draftScoreWindow->setWindowState(Qt::WindowActive);
}


void DraftHandler::debugSynergiesSet(const QString &set)
{
    synergyHandler->debugSynergiesSet(set);
}


void DraftHandler::debugSynergiesCode(const QString &code)
{
    synergyHandler->debugSynergiesCode(code);
}


void DraftHandler::testSynergies()
{
    synergyHandler->testSynergies();
}


//Construir json de HearthArena (Ya no lo usamos)
//1) Copiar line (var cards = ...)
//EL RESTO LO HACE EL SCRIPT
//2) Eliminar al principio ("\")
//3) Eliminar al final (\"";)
//4) Eliminar (\\\\\\\") Problemas con " en descripciones.
//(Ancien Spirit - Chaman)
//(Explorer's Hat - Hunter)
//(Soul of the forest - Druid)
//5) Eliminar todas las (\)

//Heroes
//01) Warrior
//02) Shaman
//03) Rogue
//04) Paladin
//05) Hunter
//06) Druid
//07) Warlock
//08) Mage
//09) Priest
