// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_QT_GAMESPAGE_H
#define VERGE_QT_GAMESPAGE_H

#include <QStackedWidget>
#include <QWidget>

class QLabel;
class PongWidget;
class QPushButton;
class SpaceInvadersWidget;
class TetrisWidget;
class QVBoxLayout;

class GamesPage : public QWidget
{
public:
    explicit GamesPage(QWidget* parent = nullptr);

private:
    void ensureSpaceInvadersPage();
    void ensurePongPage();
    void ensureTetrisPage();
    void showLibrary();
    void showPong();
    void showSpaceInvaders();
    void showTetris();

    QStackedWidget* m_stack{nullptr};
    QWidget* m_libraryPage{nullptr};
    QWidget* m_spaceInvadersPage{nullptr};
    QWidget* m_pongPage{nullptr};
    QWidget* m_tetrisPage{nullptr};
    QVBoxLayout* m_spaceInvadersLayout{nullptr};
    QVBoxLayout* m_pongLayout{nullptr};
    QVBoxLayout* m_tetrisLayout{nullptr};
    QLabel* m_titleLabel{nullptr};
    QPushButton* m_backButton{nullptr};
    SpaceInvadersWidget* m_spaceInvadersWidget{nullptr};
    PongWidget* m_pongWidget{nullptr};
    TetrisWidget* m_tetrisWidget{nullptr};
};

#endif // VERGE_QT_GAMESPAGE_H
