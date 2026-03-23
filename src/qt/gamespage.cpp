// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/gamespage.h>

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
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(24, 24, 24, 24);
    outerLayout->setSpacing(16);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    m_backButton = new QPushButton(tr("Back"), this);
    m_backButton->setVisible(false);
    m_backButton->setFixedHeight(40);
    QObject::connect(m_backButton, &QPushButton::clicked, this, [this]() { showLibrary(); });
    headerLayout->addWidget(m_backButton, 0, Qt::AlignLeft);

    m_titleLabel = new QLabel(tr("Games"), this);
    m_titleLabel->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 700;"));
    headerLayout->addWidget(m_titleLabel, 1, Qt::AlignLeft);
    headerLayout->addStretch();

    outerLayout->addLayout(headerLayout);

    m_stack = new QStackedWidget(this);
    outerLayout->addWidget(m_stack, 1);

    m_libraryPage = new QWidget(this);
    auto* libraryLayout = new QVBoxLayout(m_libraryPage);
    libraryLayout->setContentsMargins(0, 0, 0, 0);
    libraryLayout->setSpacing(18);

    auto* introLabel = new QLabel(
        tr("Pick a game to play inside Verge Core."),
        m_libraryPage);
    introLabel->setWordWrap(true);
    introLabel->setStyleSheet(QStringLiteral("font-size: 14px;"));
    libraryLayout->addWidget(introLabel);

    auto* tetrisButton = new QPushButton(QIcon(QStringLiteral(":/icons/games")), tr("Tetris"), m_libraryPage);
    tetrisButton->setIconSize(QSize(40, 40));
    tetrisButton->setMinimumHeight(84);
    tetrisButton->setStyleSheet(QStringLiteral("text-align: left; padding: 14px 18px; font-size: 18px;"));
    QObject::connect(tetrisButton, &QPushButton::clicked, this, [this]() { showTetris(); });
    libraryLayout->addWidget(tetrisButton);
    libraryLayout->addStretch();

    m_tetrisPage = new QWidget(this);
    auto* tetrisLayout = new QVBoxLayout(m_tetrisPage);
    tetrisLayout->setContentsMargins(0, 0, 0, 0);
    tetrisLayout->setSpacing(12);

    auto* helpLabel = new QLabel(
        tr("Controls: Left/Right to move, Up to rotate, Down to drop faster, P to pause, Space to restart."),
        m_tetrisPage);
    helpLabel->setWordWrap(true);
    tetrisLayout->addWidget(helpLabel);

    m_tetrisWidget = new TetrisWidget(m_tetrisPage);
    tetrisLayout->addWidget(m_tetrisWidget, 1, Qt::AlignCenter);

    m_stack->addWidget(m_libraryPage);
    m_stack->addWidget(m_tetrisPage);
    showLibrary();
}

void GamesPage::showLibrary()
{
    m_stack->setCurrentWidget(m_libraryPage);
    m_titleLabel->setText(tr("Games"));
    m_backButton->setVisible(false);
}

void GamesPage::showTetris()
{
    m_stack->setCurrentWidget(m_tetrisPage);
    m_titleLabel->setText(tr("Games / Tetris"));
    m_backButton->setVisible(true);
    if (m_tetrisWidget) {
        m_tetrisWidget->setFocus();
    }
}
