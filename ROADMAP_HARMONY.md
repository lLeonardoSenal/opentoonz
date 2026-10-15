# Roadmap: ferramentas inspiradas no Toon Boom Harmony

Fork de trabalho do OpenToonz. Cada item é uma branch `feat/<nome>`; o GitHub
Actions gera um build portátil para Windows a cada envio.

| Status | Item | Área | Branch |
|---|---|---|---|
| Feito | Build automático do Windows nas branches `feat/**` | Pipeline | `feat/ci-builds` |
| Em teste | Mesa de luz (Light Table): esmaece as outras colunas | Frame-by-frame | `feat/light-table` |
| Em teste | Referência 3D: modelo do Blender (.glb/.obj) atrás dos desenhos, com câmera animada por keyframes | Layout / perspectiva | `feat/3d-reference` |
| Em teste | Interface nova, fase 1: busca de comandos (Ctrl+Shift+K), tema e salas no estilo Clip Studio Paint | Interface | `feat/ui-structure` |
| Planejado | Interface nova, fase 2: painéis Subferramenta e Propriedade da ferramenta completos | Interface | - |
| Planejado | Interface nova, fase 3: ícones e acabamento visual | Interface | - |
| Planejado | Editor de nodes no nível do DaVinci Resolve | Nodes | - |
| Ideia | Caçador de furos de cor (próximo frame com área sem cor) | Frame-by-frame | - |
| Ideia | Timing chart anime (key / breakdown / intervalo) | Frame-by-frame | - |
