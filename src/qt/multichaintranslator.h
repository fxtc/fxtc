// Copyright (c) 2020 The FxTC Core developers

#ifndef BITCOIN_MULTICHAINTRANSLATOR_H
#define BITCOIN_MULTICHAINTRANSLATOR_H

#include <util/system.h>

#include <QTranslator>

static const char* const substitutions[] = {
    "bitcoin:",
    "bitcoins",
    "bitcoin",
    "Bitcoins",
    "Bitcoin",
    "BITCOINS"
    "BITCOIN"
};

class CMultichainTranslator : public QTranslator
{
public:
    virtual QString translate(const char* context, const char* sourceText, const char* disambiguation = nullptr, int n = -1) const override
    {
        std::string multichainContext = "Multichain_Bitcoin";
        switch (blockchainNetwork) {
            case BIP44_FIXEDTRADECOIN:
                multichainContext = "Multichain_FixedTradeCoin";
                break;
        }

        QString translatedText = QTranslator::translate(context, sourceText, disambiguation, n);
        if (translatedText.isEmpty()) translatedText = sourceText;

        for(const char* substitution : substitutions)
        {
            // Search for localized text if exists, otherwise search for original text
            QString before = QTranslator::translate("Multichain_Bitcoin", substitution);
            if (before.isEmpty() || translatedText == sourceText) before = substitution;
            QString after = QTranslator::translate(multichainContext.c_str(), before.toStdString().c_str());
            if (!after.isEmpty()) translatedText = translatedText.replace(before, after);

            // Search for original text if localized text was searched previously
            if (before != substitution) {
                before = substitution;
                QString after = QTranslator::translate(multichainContext.c_str(), before.toStdString().c_str());
                if (!after.isEmpty()) translatedText = translatedText.replace(before, after);
            }
        }

        return translatedText;
    }

private:
};

#endif // BITCOIN_MULTICHAINTRANSLATOR_H
