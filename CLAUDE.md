# CLAUDE.md: fork OpenToonz → ferramentas estilo Toon Boom Harmony

Este é o fork `lLeonardoSenal/opentoonz` do OpenToonz.

**O objetivo** é deixar o OpenToonz robusto, com ferramentas relevantes para o mercado, tomando como referência o **Toon Boom Harmony** (líder de mercado).

**A divisão de papéis:**
- Claude programa.
- O usuário decide o que entra e testa.

## Sobre o usuário

- É artista/animador 2D, iniciante a intermediário, focado em animação estilizada "vibe anime". O ponto forte dele é visdev, e ele usa o Clip Studio no dia a dia.
- **Não programa e não sabe usar o GitHub.** Nunca peça para ele rodar git, compilar ou mexer no GitHub. Tudo isso é com você.
- Usa **Windows 11**.
- Quer que você aja como **professor/instrutor**:
  - Responda em **português**, com exatidão.
  - Explique primeiro o que muda para o animador. Depois, em poucas linhas, como funciona por dentro.

## Fluxo de trabalho (siga sempre)

1. **Diagnóstico.** Leia o código da área e use `grep` para conferir se a ideia já existe, muitas vezes com outro nome:
   - Stage schematic equivale a pegs.
   - Plastic tool equivale a deformers.
   - Macro FX equivale a grupos de nós.
   - Shift and Trace já existe.
   - Onion skin já tem fade e cores.
   - O fill já tem "close gap" e "unpainted only".
   - Cell marks já existem.
2. **Proposta.** Ofereça de 2 a 4 opções com `AskUserQuestion`. Para cada uma, informe:
   - o ganho para o animador;
   - o equivalente no Harmony;
   - a dificuldade (P, M ou G).

   Recomende uma. **Não implemente nada sem a escolha do usuário.**
3. **Implementação.** Crie uma branch `feat/<nome>` a partir de `master`, com um tema por branch. Respeite:
   - o estilo do código vizinho;
   - recursos que mudam fluxos existentes devem ser **opcionais**;
   - **nunca** alterar o render de cenas antigas;
   - nada de código ou assets incompatíveis com a licença BSD;
   - nada copiado do Harmony: inspire-se só no conceito.
4. **Build.** Faça push da branch. O GitHub Actions compila sozinho. Veja os detalhes abaixo.
5. **Teste.** Mande ao usuário:
   - o link do .zip do build de Windows;
   - um roteiro de teste em passos de animador, como "abra uma cena, crie 2 colunas, ative X, espere ver Y".
6. **Registro.** O commit deve explicar o que mudou e por quê, e trazer `AI-assisted: Claude`, conforme `doc/ai_assisted_development_checklist.md`. Atualize também o `ROADMAP_HARMONY.md`.
7. **Próximo passo.** Termine sempre sugerindo de 2 a 3 próximos itens do roadmap.

## Build e entrega

- Compilar localmente exige Qt 5.15, Boost, OpenCV e libmypaint. Na primeira sessão em que isso não funcionar (sem acesso a pacotes), use o CI.
- `.github/workflows/workflow_windows.yml` roda em `master`, em `feat/**` e manualmente (`workflow_dispatch`). Ele gera o artifact `Opentoonz-Windows-<sha>`, um OpenToonz portátil pronto para abrir. Os workflows de Linux e macOS também rodam e servem como checagem extra de compilação.
- Para acompanhar:
  - `gh run list -R lLeonardoSenal/opentoonz`
  - `gh run view <id> -R lLeonardoSenal/opentoonz`
- Link do artifact para o usuário: `https://github.com/lLeonardoSenal/opentoonz/actions/runs/<id>`, na seção *Artifacts*. Ele precisa estar logado no GitHub para baixar.
- Se o build falhar: leia o log (`gh run view <id> --log-failed`), corrija e reenvie. Nunca entregue um build quebrado.
- Commits devem usar o autor `lLeonardoSenal`.

## Mapa do código (`toonz/sources/`)

| Pasta | Conteúdo |
|---|---|
| `tnztools/` | Ferramentas de desenho: brush, fill, shifttrace, skeleton, plastic, hook, assistants |
| `toonz/` | App principal: xsheet, schematic, export, render, lipsync, menus (`menubar.cpp`), ações (`mainwindow.cpp`, `menubarcommandids.h`), viewer (`sceneviewer.cpp`), preferências (`preferencespopup.cpp`) |
| `toonzlib/` | Modelo de cena, montagem do que é desenhado (`stage.cpp`), preferências (`preferences.cpp`), API de script (`scriptbinding_*`) |
| `toonzqt/` | Widgets e schematic de nós |
| `stdfx/` | Cerca de 300 arquivos de efeitos |
| `tnzext/` | Deformação plástica e malhas |
| `../../plugins/` | SDK de plugins de FX |

**Receita para adicionar um toggle de menu** (como foi feito na Mesa de luz):

1. `#define` em `menubarcommandids.h`.
2. `createToggle(...)` e `TEnv::IntVar` em `mainwindow.cpp`, mais o ramo em `onMenuCheckboxChanged`.
3. `addMenuItem` em `menubar.cpp`.
4. `ToggleCommandHandler` em `sceneviewer.cpp`.

**Receita para adicionar uma preferência:**

1. O enum em `include/toonz/preferencesitemids.h`.
2. `define(...)` em `toonzlib/preferences.cpp`.
3. O getter em `preferences.h`.
4. O rótulo, `insertUI` e `m_onEditedFuncMap` em `preferencespopup.cpp`.

## Onde paramos (14/out/2026)

- `master` = OpenToonz oficial mais o ajuste do CI.
- `feat/light-table` traz a **Mesa de luz**, já enviada:
  - comando `View → Light Table` (também no menu Draw do Ink & Paint);
  - a preferência "Onion Skin → Light Table: Opacity of Other Columns (%)", com padrão de 30%;
  - aplicada em `StageBuilder::addCell()` (`stage.cpp`);
  - afeta só o viewer, não o render, e é ignorada durante o playback no viewer 2D.

  **Pendente:** confirmar que o build compilou, entregar o .zip ao usuário com um roteiro de teste e recolher o feedback dele.
- Próximas ideias já apresentadas: "caçador de furos de cor" e "timing chart anime". O restante está em `ROADMAP_HARMONY.md`.
- As áreas priorizadas pelo usuário, todas importantes, são: frame-by-frame, composição por nós, pipeline/exportação e rig cut-out. Frame-by-frame vem primeiro.
- MCP **não** é necessário para desenvolver. Ele só faria sentido no futuro, se o usuário quiser que Claude *opere* o app pelo chat; nesse caso, o caminho seria a API de script.
