// Copyright (c) 2011-2014 The PaydayCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAYDAYCOIN_QT_PAYDAYCOINADDRESSVALIDATOR_H
#define PAYDAYCOIN_QT_PAYDAYCOINADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class PaydayCoinAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit PaydayCoinAddressEntryValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

/** PaydayCoin address widget validator, checks for a valid paydaycoin address.
 */
class PaydayCoinAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit PaydayCoinAddressCheckValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

#endif // PAYDAYCOIN_QT_PAYDAYCOINADDRESSVALIDATOR_H
