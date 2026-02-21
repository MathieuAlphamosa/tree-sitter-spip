# CLAUDE.md — tree-sitter-spip

## Projet

Grammaire tree-sitter pour le langage de squelettes SPIP (CMS français).
Utilisée par l'extension Zed `zed-spip` pour la coloration syntaxique.

## Commandes

```bash
npx tree-sitter generate   # Regénérer le parser depuis grammar.js
npx tree-sitter test        # Lancer les 29 tests du corpus
npx tree-sitter parse FILE  # Parser un fichier et afficher l'arbre
```

Toujours regénérer (`generate`) avant de tester (`test`). Le `generate` produit `src/parser.c` et `src/tree_sitter/parser.h` — ces fichiers générés doivent être commités.

## Architecture

### Fichiers clés

| Fichier | Rôle |
|---------|------|
| `grammar.js` | Définition de la grammaire (rules, externals, extras) |
| `src/scanner.c` | Scanner externe C (3 tokens : CONTENT_CHAR, SPIP_WS, SHORTHAND_LBRACE) |
| `src/parser.c` | Parser généré (ne PAS modifier à la main) |
| `test/corpus/*.txt` | Tests au format tree-sitter (input → arbre attendu) |
| `package.json` | Dépendances — tree-sitter-cli ^0.25.0 obligatoire |

### Le scanner externe (`src/scanner.c`)

Le scanner est **stateless** (pas de sérialisation d'état). Il émet 3 tokens :

1. **`CONTENT_CHAR`** — un caractère de contenu HTML/texte (pas SPIP). Le scanner vérifie via `at_spip_start()` que le caractère courant ne démarre pas une construction SPIP avant de l'émettre.

2. **`SPIP_WS`** — whitespace à l'intérieur des constructions SPIP (entre critères, entre params, etc.). N'est émis que si le whitespace est suivi de `{`, `|`, `)`, `*`, `>`, `/` (indiquant qu'on est dans un contexte SPIP).

3. **`SHORTHAND_LBRACE`** — `{` après une balise shorthand (`#TAG`). Le parser indique au scanner via `valid_symbols[SHORTHAND_LBRACE]` qu'il attend un `{` de paramètre. Sans ce mécanisme, le scanner consommerait `{` comme contenu.

### Constructions SPIP bloquées par le scanner

`at_spip_start()` bloque les séquences suivantes (elles ne deviennent jamais du contenu) :

| Séquence | Construction |
|----------|-------------|
| `(#` | Balise avec parenthèses |
| `#A-Z` | Balise shorthand |
| `#_` | Balise shorthand avec namespace de boucle |
| `<B` | Boucle `<BOUCLE_`, `<B_` |
| `<IN` | Include `<INCLURE` |
| `<mu` | Multilingual `<multi>` |
| `<:` | Traduction `<:...:>` |
| `</B` | Fermeture boucle `</BOUCLE_`, `</B_` |
| `</mu` | Fermeture multi `</multi>` |
| `<//B` | Alternative boucle `<//B_` |
| `[` | Crochet conditionnel ouvert |
| `]` | Crochet conditionnel fermé |

**Caractères NON bloqués** : `{`, `}`, `)`, `*`, `|`, `>` — ils passent comme contenu sauf quand le parser les attend dans un contexte SPIP spécifique.

## Pièges critiques et leçons apprises

### 1. Regex vides = boucle infinie WASM

**JAMAIS** utiliser `*` (zéro ou plus) dans un pattern regex de token. Toujours utiliser `+` (un ou plus). Un pattern qui matche la chaîne vide cause une boucle infinie quand compilé en WASM pour Zed.

```javascript
// MAUVAIS — boucle infinie
multi_text: (_) => /[^\[{<]*/,

// BON
multi_text: (_) => /[^\[{<]+/,
```

### 2. tree-sitter-cli version ^0.25.0 obligatoire

Les versions ^0.24.0 génèrent des headers avec l'ancienne API (`TSFieldMapSlice`, `.version`) incompatible avec le compilateur WASM de Zed. La version ^0.25.0 génère la nouvelle API (`TSMapSlice`, `.abi_version`).

Si erreur `"Failed to compile grammar 'spip'"` dans Zed → vérifier la version de tree-sitter-cli.

### 3. Le mécanisme shorthand_lbrace

Problème : les balises shorthand `#TAG{param}` ne fonctionnaient pas parce que le scanner consommait `{` comme `CONTENT_CHAR` avant que le parser puisse tenter `balise_params`.

Solution : un token externe `shorthand_lbrace` (nommé, pas caché avec `_`) qui n'est émis que quand `valid_symbols[SHORTHAND_LBRACE]` est vrai. Le parser n'active ce flag qu'après avoir matché `#TAG` dans `balise_shorthand`. Le `{` est donc capturé comme paramètre seulement dans ce contexte.

**Important** : le token est nommé `shorthand_lbrace` (sans `_` préfixe) pour qu'il soit visible dans l'arbre de syntaxe et capturable dans `highlights.scm`.

### 4. Accolades imbriquées

Les paramètres de balise, filtre, critères et include supportent les accolades imbriquées jusqu'à 3 niveaux via `param_content` → `_nested_brace_content` → `_deep_brace_content`. Ceci permet des expressions comme `{#ENV{nombre,#CONST{_MAX}}}`.

### 5. L'ordre des tokens dans `externals` compte

L'enum `TokenType` dans `scanner.c` **doit** correspondre exactement à l'ordre dans `externals` de `grammar.js` :

```javascript
// grammar.js
externals: ($) => [$._content_char, $._spip_ws, $.shorthand_lbrace]

// scanner.c
enum TokenType { CONTENT_CHAR, SPIP_WS, SHORTHAND_LBRACE };
```

### 6. Priorité du scanner

Dans `scanner_scan()`, l'ordre de vérification est critique :
1. `SHORTHAND_LBRACE` en premier (priorité haute, contexte spécifique)
2. `SPIP_WS` ensuite (whitespace dans constructions SPIP)
3. `CONTENT_CHAR` en dernier (fallback)

Si `CONTENT_CHAR` était vérifié avant `SHORTHAND_LBRACE`, le `{` serait toujours consommé comme contenu.

## Structure de la grammaire

```
template
├── comment           [(#REM) ... ]
├── loop_open         <BOUCLE_name(TYPE){criteria}>
├── loop_close        </BOUCLE_name>
├── loop_conditional_open   <B_name>
├── loop_conditional_close  </B_name>
├── loop_alternative        <//B_name>
├── include_tag       <INCLURE{...} /> ou <INCLURE{...}>
├── multi_block       <multi>[fr]...[en]...</multi>
├── translation       <:module:string:>
├── balise            (#TAG{param}|filter{param})
├── balise_shorthand  #TAG{param}
├── conditional_open  [
├── conditional_close ]
└── content           tout le reste (HTML, texte)
```

## Syntaxe SPIP de référence

Documentation officielle : https://www.spip.net/fr_article899.html

### Balises
- `#TAG` — shorthand (sans parenthèses)
- `(#TAG)` — forme standard
- `(#TAG{param1, param2})` — avec paramètres
- `(#TAG|filtre1|filtre2{param})` — avec filtres
- `#TAG*` / `#TAG**` — sortie brute (bypass typographie)
- `#_boucle:TAG` — référence explicite à une boucle parente

### Boucles
- `<BOUCLE_nom(TYPE){critere1}{critere2}>` — ouverture
- `</BOUCLE_nom>` — fermeture
- `<B_nom>` / `</B_nom>` — conditionnel "si résultats"
- `<//B_nom>` — alternative "si pas de résultats"

### Autres
- `<INCLURE{fond=chemin}{param=val} />` ou `<INCLURE{...}>`
- `<multi>[fr]texte[en]text</multi>`
- `<:module:chaine:>` — chaîne de traduction
- `[(#REM) commentaire ]`
- `[texte avant (#BALISE) texte après]` — affichage conditionnel

## Ajout d'une nouvelle construction

1. Ajouter la rule dans `grammar.js`
2. Si la construction commence par un nouveau caractère/séquence, l'ajouter dans `at_spip_start()` du scanner
3. Ajouter des tests dans `test/corpus/`
4. `npx tree-sitter generate && npx tree-sitter test`
5. Mettre à jour `highlights.scm` dans `zed-spip`
6. Supprimer `grammars/` dans `zed-spip` pour forcer la recompilation
7. Mettre à jour le `rev` dans `zed-spip/extension.toml` (hash complet 40 caractères)
