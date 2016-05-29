#include "miniongraphicsitem.h"
#include "../utility.h"
#include <QtWidgets>

MinionGraphicsItem::MinionGraphicsItem(QString code, int id, bool friendly, bool playerTurn)
{
    this->code = code;
    this->id = id;
    this->friendly = friendly;
    this->hero = false;
    this->attack = this->origAttack = Utility::getCardAtribute(code, "attack").toInt();
    this->health = this->origHealth = Utility::getCardAtribute(code, "health").toInt();
    this->damage = 0;
    this->shield = false;
    this->taunt = false;
    this->stealth = false;
    this->frozen = false;
    this->windfury = false;
    this->charge = false;
    this->exausted = true;
    this->dead = false;
    this->playerTurn = playerTurn;

    foreach(QJsonValue value, Utility::getCardAtribute(code, "mechanics").toArray())
    {
        processTagChange(value.toString(), "1");
    }
}


MinionGraphicsItem::MinionGraphicsItem(MinionGraphicsItem *copy)
{
    this->code = copy->code;
    this->id = copy->id;
    this->friendly = copy->friendly;
    this->hero = copy->hero;
    this->attack = copy->attack;
    this->origAttack = copy->origAttack;
    this->health = copy->health;
    this->origHealth = copy->origHealth;
    this->damage = copy->damage;
    this->shield = copy->shield;
    this->taunt = copy->taunt;
    this->stealth = copy->stealth;
    this->frozen = copy->frozen;
    this->windfury = copy->windfury;
    this->charge = copy->charge;
    this->exausted = copy->exausted;
    this->dead = copy->dead;
    this->playerTurn = copy->playerTurn;
    this->setPos(copy->pos());

    foreach(Addon addon, copy->addons)
    {
        this->addons.append(addon);
    }
}


int MinionGraphicsItem::getId()
{
    return this->id;
}


QString MinionGraphicsItem::getCode()
{
    return this->code;
}


bool MinionGraphicsItem::isFriendly()
{
    return this->friendly;
}


bool MinionGraphicsItem::isDead()
{
    return this->dead;
}


bool MinionGraphicsItem::isHero()
{
    return this->hero;
}


void MinionGraphicsItem::checkDownloadedCode(QString code)
{
    bool needUpdate = false;

    if(this->code == code)  needUpdate = true;
    foreach(Addon addon, this->addons)
    {
        if(addon.code == code)   needUpdate = true;
    }

    if(needUpdate)  this->update();
}


void MinionGraphicsItem::setPlayerTurn(bool playerTurn)
{
    this->playerTurn = playerTurn;
    update();
}


void MinionGraphicsItem::setDead(bool value)
{
    this->dead = value;
    update();
}


void MinionGraphicsItem::addAddon(QString code, int id, int number)
{
    Addon addon;
    addon.code = code;
    addon.id = id;
    addon.number = number;
    this->addAddon(addon);
}


//Paso por valor para almacenar una copia de addon
void MinionGraphicsItem::addAddon(Addon addon)
{
    foreach(Addon storedAddon, this->addons)
    {
        //Solo un addon por id fuente, sino un hechizo con objetivo que causa damage pondria 2 addons (objetivo y damage)
        if(storedAddon.id == addon.id)  return;
    }

    this->addons.append(addon);
    update();
}


void MinionGraphicsItem::changeZone(bool playerTurn)
{
    this->friendly = !this->friendly;
    this->playerTurn = this->friendly && playerTurn;
    if(friendly && charge)      this->exausted = false;
    else                        this->exausted = true;
}


QRectF MinionGraphicsItem::boundingRect() const
{
    return QRectF( -WIDTH/2, -HEIGHT/2, WIDTH, HEIGHT);
}


void MinionGraphicsItem::setZonePos(bool friendly, int pos, int minionsZone)
{
    const int hMinion = HEIGHT-5;
    const int wMinion = WIDTH-5;
    int x = wMinion*(pos - (minionsZone-1)/2.0);
    int y = friendly?hMinion/2:-hMinion/2;
    this->setPos(x, y);
}


void MinionGraphicsItem::processTagChange(QString tag, QString value)
{
    qDebug()<<"TAG CHANGE -->"<<id<<tag<<value;
    if(tag == "DAMAGE")
    {
        int newDamage = value.toInt();
        //Evita addons provocado por cambio de damage al morir(en el log los minion vuelven a damage 0 justo antes de morir)
        if(this->damage >= this->health && newDamage == 0)  this->dead = true;
        else                                                this->damage = newDamage;
    }
    else if(tag == "ATK")
    {
        this->attack = value.toInt();
    }
    else if(tag == "HEALTH")
    {
        this->health = value.toInt();
    }
    else if(tag == "EXHAUSTED")
    {
        this->exausted = (value=="1");
    }
    else if(tag == "DIVINE_SHIELD")
    {
        this->shield = (value=="1");
    }
    else if(tag == "TAUNT")
    {
        this->taunt = (value=="1");
    }
    else if(tag == "CHARGE")
    {
        this->charge = (value=="1");
        if(charge)    this->exausted = false;
    }
    else if(tag == "STEALTH")
    {
        this->stealth = (value=="1");
    }
    else if(tag == "FROZEN")
    {
        this->frozen = (value=="1");
    }
    else if(tag == "WINDFURY")
    {
        this->windfury = (value=="1");
    }
    else
    {
        return;
    }
    update();
}


void MinionGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    Q_UNUSED(option);

    //Card background
    painter->setBrush(QBrush(QPixmap(Utility::hscardsPath() + "/" + this->code + ".png")));
    painter->setBrushOrigin(QPointF(100,191));
    painter->drawEllipse(QPointF(0,0), 50, 68);

    //Stealth
    if(this->stealth)
    {
        painter->drawPixmap(-52, -71, QPixmap(":Images/bgMinionStealth.png"));
    }

    //Taunt/Frozen/Minion template
    bool glow = (!exausted && !frozen && (playerTurn==friendly) && attack>0);
    if(this->taunt)
    {
        painter->drawPixmap(-70, -96, QPixmap(":Images/bgMinionTaunt" + QString(glow?"Glow":"Simple") + ".png"));

        if(this->frozen)        painter->drawPixmap(-76, -82, QPixmap(":Images/bgMinionFrozen.png"));
        else                    painter->drawPixmap(-70, -80, QPixmap(":Images/bgMinionSimple.png"));
    }
    else
    {
        if(this->frozen)        painter->drawPixmap(-76, -82, QPixmap(":Images/bgMinionFrozen.png"));
        else                    painter->drawPixmap(-70, -80, QPixmap(":Images/bgMinion" + QString(glow?"Glow":"Simple") + ".png"));
    }



    //Attack/Health
    QFont font("Belwe Bd BT");
    font.setPixelSize(45);
    font.setBold(true);
    font.setKerning(true);
#ifdef Q_OS_WIN
    font.setLetterSpacing(QFont::AbsoluteSpacing, -2);
#else
    font.setLetterSpacing(QFont::AbsoluteSpacing, -1);
#endif
    painter->setFont(font);
    QPen pen(BLACK);
    pen.setWidth(2);
    painter->setPen(pen);

    if(attack>origAttack)   painter->setBrush(GREEN);
    else                    painter->setBrush(WHITE);
    QFontMetrics fm(font);
    int textWide = fm.width(QString::number(attack));
    int textHigh = fm.height();
    QPainterPath path;
    path.addText(-35 - textWide/2, 46 + textHigh/4, font, QString::number(attack));
    painter->drawPath(path);

    if(damage>0)                painter->setBrush(RED);
    else if(health>origHealth)  painter->setBrush(GREEN);
    else                        painter->setBrush(WHITE);
    textWide = fm.width(QString::number(health-damage));
    path = QPainterPath();
    path.addText(34 - textWide/2, 46 + textHigh/4, font, QString::number(health-damage));
    painter->drawPath(path);

    //Shield
    if(this->shield)
    {
        painter->drawPixmap(-71, -92, QPixmap(":Images/bgMinionShield.png"));
    }

    //Dead
    if(this->dead)
    {
        painter->drawPixmap(-87/2, -94/2, QPixmap(":Images/bgMinionDead.png"));
    }

    //Addons
    for(int i=0; i<this->addons.count() && i<4; i++)
    {
        QString addonCode = this->addons[i].code;
        int moveX, moveY;
        switch(i)
        {
            case 0:
                moveX = 0;
                moveY = 10;
                break;
            case 1:
                moveX = 0;
                moveY = -40;
                break;
            case 2:
                moveX = -25;
                moveY = -15;
                break;
            case 3:
                moveX = 25;
                moveY = -15;
                break;
        }

        if(addonCode == "FATIGUE")
        {
            painter->drawPixmap(moveX-32, moveY-32, QPixmap(":Images/bgFatigueAddon.png"));
        }
        else
        {
            painter->setBrush(QBrush(QPixmap(Utility::hscardsPath() + "/" + addonCode + ".png")));
            painter->setBrushOrigin(QPointF(100+moveX,202+moveY));
            painter->drawEllipse(QPointF(moveX,moveY), 32, 32);
        }

        painter->drawPixmap(moveX-35, moveY-35, QPixmap(":Images/bgMinionAddon.png"));
    }
}
