APK Icon Editor Reborn is translated at [Crowdin](https://crowdin.com/project/apk-icon-editor-reborn).

The Reborn fork uses its own localization project. The original APK Icon Editor Crowdin project is not used for new Reborn strings.

## Files

- `apk-icon-editor.ts` is the English source file generated from the Qt source tree.
- `translations/apk-icon-editor.<lang>.ts` files are imported translations used as the initial Crowdin baseline.
- Runtime `.qm` files are still deployed from `deploy/general/lang`.

## Crowdin setup

Create a Crowdin project with settings matching the original project as closely as possible:

- name: `APK Icon Editor Reborn`;
- identifier/slug: `apk-icon-editor-reborn`;
- description: `APK Icon Editor Reborn is a cross-platform APK resource editor and a maintained fork of APK Icon Editor. Help translate the application interface.`;
- visibility: public open-source project;
- source language: English;
- file format: Qt Linguist TS;
- target languages: Chinese, Dutch, French, German, Greek, Hungarian, Italian, Portuguese, Romanian, Russian, Spanish, Turkish;
- project URL: `https://crowdin.com/project/apk-icon-editor-reborn`.

Crowdin does not provide a GitHub-style project fork. Create a new project, upload `apk-icon-editor.ts`, then upload existing translation files from `lang/translations`.

Suggested language mapping:

- `de` - German;
- `el` - Greek;
- `es` - Spanish;
- `fr` - French;
- `hu` - Hungarian;
- `it` - Italian;
- `nl` - Dutch;
- `pt` - Portuguese;
- `ro` - Romanian;
- `ru` - Russian;
- `tr` - Turkish;
- `zh` - Chinese.

## Sync

The repository contains `crowdin.yml` and a manual GitHub workflow. Configure repository secrets:

- `CROWDIN_PROJECT_ID`
- `CROWDIN_PERSONAL_TOKEN`

Then run the `Crowdin localization` workflow manually.

Manual CLI equivalent:

```sh
crowdin upload sources
crowdin upload translations
crowdin download translations
```

To refresh source strings locally, run Qt Linguist `lupdate` against the source tree and write the result to `lang/apk-icon-editor.ts`.
