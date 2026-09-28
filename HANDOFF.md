# HANDOFF.md -- ore 專案交接文檔

> 版本:v1.3.6 | 日期:2026-09-28 | 狀態:設計閉合,離線驗證 100% 覆蓋,待硬體驗證鏈
>
> **一句話**:ore 是一個從 SHA-256d 數學到商業路線圖全鏈路閉合的開源 PoW 矽平台
> 參考設計;所有能在無晶圓環境驗證的東西都已驗證並留證(REGRESSION.md)。

## 1. 專案全貌

| 維度 | 內容 | 狀態 |
|---|---|---|
| 產品定位 | 開放、可審計、可重定向的 PoW 硬體平台(非「更差的 Antminer」) | 見 BUSINESS.md |
| 產品線 | A 研究開發套件 / B PoW 研究平台 / C 廢熱礦機(有條件)/ D 服務 | 見 PRODUCTS.md |
| 硬體 | Rocket(主推)+ BOOM 雙配置;8~64 引擎;MMIO+IRQ;thermal/power regs | v1.3.4 起相容 chipyard main |
| 軟體 | stratum v1(已驗證)/v2 骨架、dvfs(Pareto→bandit→MPC 譜系)、ML 四件 | 全部主機驗證 |
| 商業 | 2026 全陣營對比、能量模型(J/TH=128×E_round)、四道量化閘門 | 錨定公開數據 |
| GitHub | 倉名 `ore`(被佔則 `ore-soc`),tag v1.0.0–v1.3.6 | **待 push** |

## 2. 驗證狀態矩陣(接手時照此核對)

| 層 | 項目 | 狀態 | 證據 |
|---|---|---|---|
| 數學 | midstate / state3 / endianness(三層) / genesis KAT | ✅ 已驗證 | tests/golden.py 全綠 |
| 數學 | CSA 精確性、FA 帳 | ✅ 已驗證 | tests/csa_model.py 20k 向量 |
| 經濟 | 能量模型 + 公開錨點(S9/BZM2/BM1370) | ✅ 已驗證 | tapeout/energy_model.py |
| 軟體 | stratum 7 項 cross-check(hashlib) | ✅ 已驗證 | sw/stratum_client.c -DSTRATUM_TEST |
| 軟體 | dvfs/bandit/EWMA/MPC/guard | ✅ 已驗證 | 各 *_TEST 主機測試 |
| 資料 | 向量 JSON、Scala 括號平衡 | ✅ 已驗證 | REGRESSION.md |
| RTL | pipe/引擎 chiseltest | ⏳ **待你的機器** | 向量已備(vectors.json/genesis_kat.json) |
| 系統 | elaboration + genesis smoke(Verilator) | ⏳ 待你的機器 | miner_baremetal.c 就緒 |
| 硬體 | 綜合/後端/流片 | ⏳ 未開始 | 路線在 PRODUCTS.md |

## 3. 接手後 72 小時行動清單

1. **push GitHub**(30 分):解 bundle → `git remote add origin git@github.com:<you>/ore.git`
   → `git push -u origin main --tags`;補描述與 topics(README 頂部有模板)。
2. **起環境**(1–2 小時):Docker 路線 `docker build -t ore-env . && docker run --memory=12g --cpus=8 -it ore-env`
   (或 `bash scripts/install_chipyard.sh`,Ubuntu 直裝)。
3. **跑驗證鏈**(2–4 小時,含首次編譯):
   `scripts/integrate_into_chipyard.sh ~/chipyard`
   → `sbt "testOnly mining.Sha256PipeTest mining.MiningEngineTest"`
   → `make CONFIG=BoomMiningConfig`(elaboration)
   → `make CONFIG=RocketMiningConfig run-binary BINARY=miner_baremetal.riscv`
4. **遇錯**:先查 `docs/CHIPYARD_INTEGRATION.md` 第六節(first-compile troubleshooting,
   來自真實編譯嘗試的 6 類已知坑)。仍紅 → 把 `[error]` 行貼回來。
5. **全綠**:`git tag v2.0`,更新 PRODUCTS.md Phase 0 為 DONE,開始 G0 之後的計畫。

## 4. 關鍵決策記錄(ADR 摘要)

| 決策 | 選擇 | 理由(一句話) |
|---|---|---|
| 控制核心 | Rocket 主推,BOOM 留 bring-up | 官方 small core 已極簡;面積留給引擎 |
| 引擎規模 | 模擬 8 / tapeout 32–64 | 固定功耗分攤:8 引擎繳 2.5× 稅,64 引擎 1.05× |
| 能效策略 | V² + full-custom,不追 3nm | J/TH=128×E_round;28nm 只是驗證載具 |
| 產品線 C | 有條件存在(G3:16nm ≤100 J/TH 且廢熱 LCOE 優 20%) | 不達就砍,不沉沒成本 |
| AI | Tier1 全做(免費)、Tier2 Gemmini 選配(Phase 2 閘門)、Tier3 拒絕 | Bitmain Sophon 前車之鑑 |
| 命名 | ore | 不綁演算法、有吉祥物、避開 Bitaxe 家族 |
| 開放性 | 核心永遠開源(Apache-2.0) | 開放性本身就是產品 |

## 5. 已知問題與技術債(誠實清單)

1. **RTL 未過真實編譯**:chiseltest/elaboration 待跑;嫌疑清單已備(Chisel 6 禁
   `eng.clock :=` 排第一)。LowPowerAdditions/FullCustomStage/ThermalRegs/PowerRegs
   是 SKETCH,掛 chiseltest 閘門。
2. **SV2 client 未對官方 test vectors**(骨架可編譯,協議細節待 sv2-spec 裁判)。
3. **gemmini 依賴**:AiConfig 在 hw/optional/,整合腳本預設排除;啟用前先補 gemmini 子模組。
4. **經濟模型一階**:D+90 要用 PTPX/SAIF 實測值替換 E_round 估計(reconciliation 文件有裁決規則)。
5. **沙盒限制**:開發環境 5GB/會重置/無 root——所有工作已轉為「你機器三條命令」模式,
   `scripts/backup_chipyard_env.sh` 可在你的機器上建立可搬遷環境。

## 6. 專案統計(歷次会话累計)

- 版本:16 個 tag(v1.0.0→v1.3.6),255+ 檔案
- 被抓出的 bug:9+ 個設計/實現錯誤(8 個寫進 README Lessons,含 endianness 兩層、
  SHA-256 長度欄位 BE、JSON parser 三連、hex2bin UB、state3 add-back 陷阱),全部死於
  golden cross-check,無一靠 review 存活
- 決策閘門:G0–G3 四道,全部量化、全部附 kill criteria
- 文件:11 份技術 + 4 份商業 + REGRESSION attestation

## 7. 命令速查

    git clone ore_v1.3.6.bundle ore          # 還原完整歷史
    python3 tests/golden.py                   # 全部 golden selftest
    bash scripts/integrate_into_chipyard.sh   # 一鍵整合進 chipyard
    bash scripts/backup_chipyard_env.sh       # 編譯完打包環境(可搬遷)
