# Push to GitHub

The bundle contains a complete git repository (branch `main`, tag
`v1.0.0`, 7 commits). To publish:

    # 1. create an EMPTY repo on github.com (no README/license -- avoid
    #    a merge conflict on first push), then:
    git clone /path/to/extracted/mining_soc  mining_soc
    cd mining_soc
    git remote add origin git@github.com:<you>/mining_soc.git
    git push -u origin main
    git push origin v1.0.0

    # or without cloning, from the extracted directory:
    git remote add origin git@github.com:<you>/mining_soc.git
    git push -u origin main --tags

# Releasing

    git tag -a v1.1.0 -m "..." && git push origin v1.1.0
    # attach release notes; GitHub renders README.md on the repo page.

# Repo settings suggestions

- Topics: riscv, boom, rocket, chisel, sha256, bitcoin-mining, asic,
  open-hardware, chipyard, stratum-v2
- License detection: Apache-2.0 (LICENSE present).
- Branch protection on `main` (require PR + CI when public CI exists).
- Mirror backup: `git bundle create mining_soc.bundle --all` -- a single
  file that contains the full history; restore with
  `git clone mining_soc.bundle`.
