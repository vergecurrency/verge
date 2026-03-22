// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_QT_GAMESPAGE_H
#define VERGE_QT_GAMESPAGE_H

#include <QStackedWidget>
#include <QWidget>

class QLabel;
class QPushButton;
class TetrisWidget;
class QVBoxLayout;

class GamesPage : public QWidget
{
public:
    explicit GamesPage(QWidget* parent = nullptr);

private:
    void showLibrary();
    void showTetris();

    QStackedWidget* m_stack{nullptr};
    QWidget* m_libraryPage{nullptr};
    QWidget* m_tetrisPage{nullptr};
    QLabel* m_titleLabel{nullptr};
    QPushButton* m_backButton{nullptr};
    TetrisWidget* m_tetrisWidget{nullptr};
};

#endif // VERGE_QT_GAMESPAGE_H
