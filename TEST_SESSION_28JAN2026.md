# TEST SESSION - RING SIZE 48 VALIDATION
**Date:** January 28, 2026  
**Daemon Mode:** Offline (--offline flag)  
**Objective:** Validate Ring Size 48 with various transaction amounts

---

## 🎯 TEST RESULTS SUMMARY

All 5 tests were successful and confirmed on testnet blockchain.

| Test | Amount | Ring | Block | Fee | Split? | Status |
|------|--------|------|-------|-----|--------|--------|
| #1 | 1 HRG | 48 | 838 | 0.024 HRG | No | ✅ SUCCESS |
| #2 | 5 HRG | 48 | 840 | 0.042 HRG | No | ✅ SUCCESS |
| #3 | 10 HRG | 48 | 841 | 0.042 HRG | No | ✅ SUCCESS |
| #4 | 50 HRG | 48 | 844 | 0.104 HRG | **Yes (2 TX)** | ✅ SUCCESS |
| #5 | 100 HRG | 48 | 845 | 0.122 HRG | **Yes (2 TX)** | ✅ SUCCESS |

---

## 📊 DETAILED TEST RESULTS

### Test #1: 1 HRG
- **TXID:** 3f5ebc019211560e1d64d5d95bf317f34317e54ca3d17c9288a4fef22cfbfffa
- **Block:** 838
- **Timestamp:** 2026-01-28 15:42:04Z
- **Fee:** 0.024 HRG
- **Ring Size:** 48
- **Transaction Split:** No
- **Status:** ✅ Confirmed

### Test #2: 5 HRG
- **TXID:** aa31ec08eef81d904d621e745082231523618354fe7b0f75a957dcf6e078d24e
- **Block:** 840
- **Timestamp:** 2026-01-28 15:45:29Z
- **Fee:** 0.042 HRG
- **Ring Size:** 48
- **Transaction Split:** No
- **Status:** ✅ Confirmed

### Test #3: 10 HRG
- **TXID:** c4b7bf9f906bfe729e2925ab6e5d8db17ee319bb5fda3e0b44323e82474d9c20
- **Block:** 841
- **Timestamp:** 2026-01-28 15:46:54Z
- **Fee:** 0.042 HRG
- **Ring Size:** 48
- **Transaction Split:** No
- **Status:** ✅ Confirmed

### Test #4: 50 HRG (SPLIT)
- **TXID 1:** 32582e462513ba5e8a59b1940c1a274cec13a1f8d28506a447173027507a9b2a
- **TXID 2:** 1fb7154eead51eb8e35de6587156ea0865725bc2c16fdb01cae6f8ad4ad8a13d
- **Block:** 844
- **Timestamp:** 2026-01-28 15:48:29Z
- **Total Fee:** 0.104 HRG (0.024 + 0.080)
- **Ring Size:** 48
- **Transaction Split:** ⚠️ Yes (2 transactions)
- **Status:** ✅ Confirmed

### Test #5: 100 HRG (SPLIT)
- **TXID 1:** 682243d49db5e25454a1af33f40c868c1cbf9097b798b8c80aac7b3c6f9957fb
- **TXID 2:** d7bfb3e689595e20adf17303d069ab7bd83d85e717a68e13d6b067e8c5d36f3a
- **Block:** 845
- **Timestamp:** 2026-01-28 15:49:04Z
- **Total Fee:** 0.122 HRG (0.042 + 0.080)
- **Ring Size:** 48
- **Transaction Split:** ⚠️ Yes (2 transactions)
- **Status:** ✅ Confirmed

---

## 🔍 KEY FINDINGS

### Optimal Range (No Split)
**Amounts: 1-10 HRG**
- ✅ Single transaction
- ✅ Low fees (0.024-0.042 HRG)
- ✅ Ring Size 48 works perfectly
- ✅ No warnings or splits
- ✅ **RECOMMENDED for regular use**

### Split Required Range
**Amounts: 50-100 HRG**
- ⚠️ Transaction automatically split into 2
- ⚠️ Higher fees (0.104-0.122 HRG)
- ⚠️ 2-3x more expensive than small amounts
- ⚠️ Still functional but not optimal
- ⚠️ **Use with caution for large amounts**

---

## 💡 RECOMMENDATIONS

### For Mainnet Launch:
1. **Document ring size 48 as optimal** in user guide
2. **Recommend amounts under 10 HRG** for single transactions
3. **Warn users about splits** for amounts over 50 HRG
4. **Consider implementing auto-split warning** in wallet UI

### For Users:
- **Use ring 48** for best privacy/fee balance
- **Keep transactions under 10 HRG** to avoid splits
- **For larger amounts:** Accept split or send multiple smaller transactions

---

## 📈 FEE ANALYSIS

| Amount Range | Average Fee | Fee % | Split? |
|--------------|-------------|-------|--------|
| 1-10 HRG | 0.024-0.042 HRG | 0.24-0.84% | No |
| 50-100 HRG | 0.104-0.122 HRG | 0.11-0.21% | Yes |

**Note:** Larger amounts have lower fee percentage but require splits.

---

## 🎓 COMPARISON WITH PREVIOUS TESTS

### Session Jan 26, 2026 (Initial Tests)
- Ring 32: 10 HRG, 0.054 HRG, ⚠️ WARNING
- Ring 48: 10 HRG, 0.024 HRG, ✅ SUCCESS
- Ring 64: 10 HRG, 0.110 HRG, ⚠️ SPLIT

### Session Jan 28, 2026 (Current)
- Extended testing with multiple amounts
- Validated ring 48 across 1-100 HRG range
- Identified optimal range (1-10 HRG)
- Documented split behavior (50+ HRG)

---

## ✅ CONCLUSION

**Ring Size 48 is confirmed as OPTIMAL for Hidering:**
- ✅ Best privacy/fee balance
- ✅ Works perfectly for typical transactions (1-10 HRG)
- ✅ Predictable behavior
- ✅ No warnings for normal amounts
- ⚠️ Requires splits for large amounts (50+ HRG) but still functional

**Recommendation:** Document ring 48 as default/recommended in all user-facing materials.

---

**Test Duration:** ~10 minutes  
**Blockchain Height:** 838-845 (7 blocks)  
**Total Test Amount:** 166 HRG  
**Total Fees Paid:** 0.334 HRG

**Next Steps:** 
- Update documentation with these findings
- Consider implementing ring 48 as suggested default
- Add transaction split warnings in wallet
- Prepare for mainnet launch

---
**END OF TEST SESSION**
