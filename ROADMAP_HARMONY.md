# Roadmap: ferramentas inspiradas no Toon Boom Harmony

Cada item vira uma branch `feat/<nome>`. A cada envio, o GitHub Actions gera um build portátil para Windows.

Antes de começar um item, confira no código se ele já existe.

**Dificuldade:** P = pequena, M = média, G = grande.

## Status

| Status | Item | Área | Branch |
|---|---|---|---|
| Feito | Build automático do Windows em `master` e `feat/**` | Pipeline | (em `master`) |
| Em teste | Mesa de luz (Light Table): esmaece as outras colunas | Frame-by-frame | `feat/light-table` |

## Ideias

### Frame-by-frame (prioridade 1)

| Ideia | Descrição | Dif. |
|---|---|---|
| Caçador de furos de cor | Pula para o próximo frame que tem área fechada sem cor e destaca onde ela está | M |
| Timing chart anime | Marca cada célula como key, breakdown ou intervalo e mostra o chart na timeline e no PDF do xsheet. Usar como base os cell marks, que já existem | M–G |
| Mesa de luz por coluna | Escolher quais colunas ficam visíveis como referência | P–M |
| Line test rápido | Playback em cache com áudio e scrub | M |

### Composição por nós

| Ideia | Descrição | Dif. |
|---|---|---|
| Preset "sombra anime" | Tone ou highlight por matte da cor de referência, em um só nó | M |
| Organização da schematic | Backdrops (caixas coloridas com título), busca de nó e minimapa | M |
| Biblioteca de presets de FX | Glow, rim light, blur de profundidade | M |
| Preview do nó | Ver o resultado de um nó específico | M |

### Rig cut-out

| Ideia | Descrição | Dif. |
|---|---|---|
| Drawing substitution | Painel com miniaturas para trocar boca, mão e olhos em um clique | M |
| Curve/envelope deformer | Estender a Plastic tool | G |
| Master controller | Sliders que controlam várias pegs ou substituições ao mesmo tempo | G |
| IK simples para membros | Partir da skeleton tool | G |

### Pipeline e exportação

| Ideia | Descrição | Dif. |
|---|---|---|
| Validador de cena | Detecta nomes fora do padrão, níveis faltando e resolução ou fps fora do padrão | M |
| Presets de export por cliente | MP4, ProRes, PNG seq, EXR, inclusive em lote | M |
| Templates de cena | Resolução, camadas BG/char/FX e safe area prontas | P–M |
| API de script ampliada | Expandir `toonzlib/scriptbinding_*` para automação | M–G |
| Melhorias OCA/XDTS | Export e import para trocar arquivos com outros apps | M |
