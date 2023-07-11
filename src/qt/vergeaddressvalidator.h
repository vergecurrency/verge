// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2023 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_QT_VERGEADDRESSVALIDATOR_H
#define VERGE_QT_VERGEADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class VERGEAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit VERGEAddressEntryValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

/** VERGE address widget validator, checks for a valid verge address.
 */
class VERGEAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit VERGEAddressCheckValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

#endif // VERGE_QT_VERGEADDRESSVALIDATOR_H
