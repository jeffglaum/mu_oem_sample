#ifndef __EFI_WIFI_MGR_DXE_H__
#define __EFI_WIFI_MGR_DXE_H__

#define OUI_IEEE_80211I 0xAC0F00

typedef enum {
    Ieee80211PairwiseCipherSuiteUseGroupCipherSuite = 0,
    Ieee80211PairwiseCipherSuiteWEP40 = 1,
    Ieee80211PairwiseCipherSuiteTKIP = 2,
    Ieee80211PairwiseCipherSuiteCCMP = 4,
    Ieee80211PairwiseCipherSuiteWEP104 = 5,
    Ieee80211PairwiseCipherSuiteBIP = 6,
    //...
} IEEE_80211_PAIRWISE_CIPHER_SUITE;
#define IEEE_80211_PAIRWISE_CIPHER_SUITE_USE_GROUP \
    (OUI_IEEE_80211I | (Ieee80211PairwiseCipherSuiteUseGroupCipherSuite << 24))
#define IEEE_80211_PAIRWISE_CIPHER_SUITE_WEP40 \
    (OUI_IEEE_80211I | (Ieee80211PairwiseCipherSuiteWEP40 << 24))
#define IEEE_80211_PAIRWISE_CIPHER_SUITE_TKIP \
    (OUI_IEEE_80211I | (Ieee80211PairwiseCipherSuiteTKIP << 24))
#define IEEE_80211_PAIRWISE_CIPHER_SUITE_CCMP \
    (OUI_IEEE_80211I | (Ieee80211PairwiseCipherSuiteCCMP << 24))
#define IEEE_80211_PAIRWISE_CIPHER_SUITE_WEP104 \
    (OUI_IEEE_80211I | (Ieee80211PairwiseCipherSuiteWEP104 << 24))
#define IEEE_80211_PAIRWISE_CIPHER_SUITE_BIP \
    (OUI_IEEE_80211I | (Ieee80211PairwiseCipherSuiteBIP << 24))

typedef enum {
    Ieee80211AkmSuite8021XOrPMKSA = 1,
    Ieee80211AkmSuitePSK = 2,
    Ieee80211AkmSuite8021XOrPMKSASHA256 = 5,
    Ieee80211AkmSuitePSKSHA256 = 6
    //...
} IEEE_80211_AKM_SUITE;

#define IEEE_80211_AKM_SUITE_8021X_OR_PMKSA \
    (OUI_IEEE_80211I | (Ieee80211AkmSuite8021XOrPMKSA << 24))
#define IEEE_80211_AKM_SUITE_PSK (OUI_IEEE_80211I | (Ieee80211AkmSuitePSK << 24))
#define IEEE_80211_AKM_SUITE_8021X_OR_PMKSA_SHA256 \
    (OUI_IEEE_80211I | (Ieee80211AkmSuite8021XOrPMKSASHA256 << 24))
#define IEEE_80211_AKM_SUITE_PSK_SHA256 (OUI_IEEE_80211I | (Ieee80211AkmSuitePSKSHA256 << 24))

#endif