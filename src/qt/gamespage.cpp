// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/gamespage.h>

#include <qt/games/pongwidget.h>
#include <qt/games/spaceinvaderswidget.h>
#include <qt/games/tetriswidget.h>

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSize>
#include <QStackedWidget>
#include <QVBoxLayout>

GamesPage::GamesPage(QWidget* parent) : QWidget(parent)
{
    setObjectName("GamesPage");

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(24, 24, 24, 24);
    outerLayout->setSpacing(16);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    m_backButton = new QPushButton(tr("Back"), this);
    m_backButton->setObjectName("GamesBackButton");
    m_backButton->setVisible(false);
    m_backButton->setFixedHeight(40);
    QObject::connect(m_backButton, &QPushButton::clicked, this, [this]() { showLibrary(); });
    headerLayout->addWidget(m_backButton, 0, Qt::AlignLeft);

    m_titleLabel = new QLabel(tr("Games"), this);
    m_titleLabel->setObjectName("GamesTitle");
    headerLayout->addWidget(m_titleLabel, 1, Qt::AlignLeft);
    headerLayout->addStretch();

    outerLayout->addLayout(headerLayout);

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName("GamesStack");
    outerLayout->addWidget(m_stack, 1);

    m_libraryPage = new QWidget(this);
    m_libraryPage->setObjectName("GamesLibraryPage");
    auto* libraryLayout = new QVBoxLayout(m_libraryPage);
    libraryLayout->setContentsMargins(0, 0, 0, 0);
    libraryLayout->setSpacing(18);

    auto* introLabel = new QLabel(
        tr("Pick a game to play inside Verge Core."),
        m_libraryPage);
    introLabel->setObjectName("GamesIntroLabel");
    introLabel->setWordWrap(true);
    libraryLayout->addWidget(introLabel);

    auto* tetrisButton = new QPushButton(QIcon(QStringLiteral(":/games/icons/tetris")), tr("Tetris"), m_libraryPage);
    tetrisButton->setObjectName("GamesLauncherButton");
    tetrisButton->setIconSize(QSize(40, 40));
    tetrisButton->setMinimumHeight(84);
    QObject::connect(tetrisButton, &QPushButton::clicked, this, [this]() { showTetris(); });
    libraryLayout->addWidget(tetrisButton);

    auto* pongButton = new QPushButton(QIcon(QStringLiteral(":/games/icons/pong")), tr("Pong"), m_libraryPage);
    pongButton->setObjectName("GamesLauncherButton");
    pongButton->setIconSize(QSize(40, 40));
    pongButton->setMinimumHeight(84);
    QObject::connect(pongButton, &QPushButton::clicked, this, [this]() { showPong(); });
    libraryLayout->addWidget(pongButton);

    auto* spaceInvadersButton = new QPushButton(QIcon(QStringLiteral(":/games/icons/spaceinvaders")), tr("Space Invaders"), m_libraryPage);
    spaceInvadersButton->setObjectName("GamesLauncherButton");
    spaceInvadersButton->setIconSize(QSize(40, 40));
    spaceInvadersButton->setMinimumHeight(84);
    QObject::connect(spaceInvadersButton, &QPushButton::clicked, this, [this]() { showSpaceInvaders(); });
    libraryLayout->addWidget(spaceInvadersButton);
    libraryLayout->addStretch();

    m_spaceInvadersPage = new QWidget(this);
    m_spaceInvadersPage->setObjectName("GamesPlayPage");
    m_spaceInvadersLayout = new QVBoxLayout(m_spaceInvadersPage);
    m_spaceInvadersLayout->setContentsMargins(0, 0, 0, 0);
    m_spaceInvadersLayout->setSpacing(12);

    m_pongPage = new QWidget(this);
    m_pongPage->setObjectName("GamesPlayPage");
    m_pongLayout = new QVBoxLayout(m_pongPage);
    m_pongLayout->setContentsMargins(0, 0, 0, 0);
    m_pongLayout->setSpacing(12);

    m_tetrisPage = new QWidget(this);
    m_tetrisPage->setObjectName("GamesPlayPage");
    m_tetrisLayout = new QVBoxLayout(m_tetrisPage);
    m_tetrisLayout->setContentsMargins(0, 0, 0, 0);
    m_tetrisLayout->setSpacing(12);

    m_stack->addWidget(m_libraryPage);
    m_stack->addWidget(m_spaceInvadersPage);
    m_stack->addWidget(m_pongPage);
    m_stack->addWidget(m_tetrisPage);
    showLibrary();
}

void GamesPage::ensureSpaceInvadersPage()
{
    if (m_spaceInvadersWidget) {
        return;
    }

    auto* invadersHelpLabel = new QLabel(
        tr("Controls: Left/Right or A/D to move, Space to shoot, P to pause, R to restart."),
        m_spaceInvadersPage);
    invadersHelpLabel->setObjectName("GamesHelpLabel");
    invadersHelpLabel->setWordWrap(true);
    m_spaceInvadersLayout->addWidget(invadersHelpLabel);

    m_spaceInvadersWidget = new SpaceInvadersWidget(m_spaceInvadersPage);
    m_spaceInvadersWidget->setObjectName("GamesCanvas");
    m_spaceInvadersLayout->addWidget(m_spaceInvadersWidget, 1, Qt::AlignCenter);
}

void GamesPage::ensureTetrisPage()
{
    if (m_tetrisWidget) {
        return;
    }

    auto* helpLabel = new QLabel(
        tr("Controls: Left/Right to move, Up to rotate, Down to drop faster, P to pause, Space to restart."),
        m_tetrisPage);
    helpLabel->setObjectName("GamesHelpLabel");
    helpLabel->setWordWrap(true);
    m_tetrisLayout->addWidget(helpLabel);

    m_tetrisWidget = new TetrisWidget(m_tetrisPage);
    m_tetrisWidget->setObjectName("GamesCanvas");
    m_tetrisLayout->addWidget(m_tetrisWidget, 1, Qt::AlignCenter);
}

void GamesPage::ensurePongPage()
{
    if (m_pongWidget) {
        return;
    }

    auto* helpLabel = new QLabel(
        tr("Controls: Left/Right or A/D to move, P to pause, R to restart."),
        m_pongPage);
    helpLabel->setObjectName("GamesHelpLabel");
    helpLabel->setWordWrap(true);
    m_pongLayout->addWidget(helpLabel);

    m_pongWidget = new PongWidget(m_pongPage);
    m_pongWidget->setObjectName("GamesCanvas");
    m_pongLayout->addWidget(m_pongWidget, 1, Qt::AlignCenter);
}

void GamesPage::showLibrary()
{
    m_stack->setCurrentWidget(m_libraryPage);
    m_titleLabel->setText(tr("Games"));
    m_backButton->setVisible(false);
}

void GamesPage::showPong()
{
    ensurePongPage();
    m_stack->setCurrentWidget(m_pongPage);
    m_titleLabel->setText(tr("Games / Pong"));
    m_backButton->setVisible(true);
    if (m_pongWidget) {
        m_pongWidget->setFocus();
    }
}

void GamesPage::showSpaceInvaders()
{
    ensureSpaceInvadersPage();
    m_stack->setCurrentWidget(m_spaceInvadersPage);
    m_titleLabel->setText(tr("Games / Space Invaders"));
    m_backButton->setVisible(true);
    if (m_spaceInvadersWidget) {
        m_spaceInvadersWidget->setFocus();
    }
}

void GamesPage::showTetris()
{
    ensureTetrisPage();
    m_stack->setCurrentWidget(m_tetrisPage);
    m_titleLabel->setText(tr("Games / Tetris"));
    m_backButton->setVisible(true);
    if (m_tetrisWidget) {
        m_tetrisWidget->setFocus();
    }
}
