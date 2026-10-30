# PicoPlay Android — Agent & Development Guidelines

## 📌 Regra de Ouro: Sincronização Obrigatória em Atualizações
Todas as atualizações, novos lançamentos, bump de versões ou alterações de funcionalidades têm **obrigatoriamente** de atualizar e sincronizar a tríade:

1. **GitHub Webpage (`gh-pages` e `docs/`):**
   - O site live é servido diretamente a partir da raiz do ramo `gh-pages` (`https://birth1mark.github.io/picoplay-android/`).
   - Sempre que alterar `docs/index.html`, `docs/store_assets/`, `docs/screenshots/` ou `docs/privacy.html` no ramo `main`, **deve sincronizar e enviar imediatamente para o ramo `gh-pages`**.
   - Garantir que todos os URLs de download utilizam os endpoints canónicos sem erro 404 (`https://github.com/birth1mark/picoplay-android/releases/latest/download/...`).

2. **README (`README.md`):**
   - Atualizar a tabela de downloads e releases.
   - Atualizar badges de versão (ex: `Release-vX.Y.Z Live`).
   - Validar que 100% dos links externos retornam HTTP 200/302.
   - Atualizar instruções de compilação ou ferramentas novas (ex: `make -j$(nproc)` em `test/`, `tools/preview/preview_chassis --interactive`).

3. **Licença (`LICENSE.MD`):**
   - Manter sempre o nome de ficheiro exato `LICENSE.MD` (com `.MD` em maiúsculas).
   - Assegurar que o ano e a atribuição de autor pertencem estritamente a `birth1mark`.
   - Garantir que a ligação da licença no README aponta para `https://github.com/birth1mark/picoplay-android/blob/main/LICENSE.MD`.

---

## 🔒 Diretivas de Identidade e Conformidade
- **Autor e Identidade:** Utilizar sempre `birth1mark` (`birth1mark@users.noreply.github.com`) em todos os commits, PRs, documentação e ficheiros. Nunca divulgar nomes reais pessoais.
- **Conformidade Google Play:**
  - O sabor `playstore` inclui apenas cartuchos starter open-source limpos (sem cartuchos comerciais/terceiros).
  - Target SDK mantido no nível exigido pela Play Store (Android 16 / API 36).
  - Alinhamento de memória ELF rigorosamente verificado para compatibilidade de 16KB de página (`0x4000`).
