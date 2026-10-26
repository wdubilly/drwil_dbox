// Texts of the governance state and their layout for the agent (split from state.mjs, which
// re-exports them): full context and short reminder, built from the state already read and the disk.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { readBlock } from "./scope.mjs";
import { config } from "./briefs.mjs";
import { worktreeLines } from "./worktree.mjs";

const TEXTS = {
  fr: {
    header: "[drwil] État de gouvernance, lu sur disque (.drwil/state.json) — ne pas le déduire de la conversation.",
    missing: "(.drwil/state.json absent : état neutre)",
    disabled:
      "[drwil] Gouvernance désactivée (réglage « scopeGate » : off dans .drwil/ia-first.json) : aucune activité ni périmètre à suivre.",
    invalid: "⚠ État invalide, traité comme SCOPING neutre :",
    activity: (a) => `Activité : ${a}`,
    brief: (b) => `Attente active : ${b ?? "aucune attente active"}`,
    request: (r) => `Demande active : ${r ?? "aucune"}`,
    autoMerge:
      "Fusion automatique autorisée au lancement : en CLOSING, après verify PASS et le push de la clôture, dans cet ordre : attendre que GitHub ait calculé la PR (`gh pr view --json mergeStateStatus`) et, si elle est en retard sur la branche principale (BEHIND), la mettre à jour ; programmer `gh pr merge --auto --merge` ; vérifier que la fusion est bien programmée (`gh pr view --json autoMergeRequest`) ; suivre la PR par `node .githooks/pr-watch.mjs <numéro>`, relancé tant qu'il dit « verte » sans « fusionnée », 30 minutes au plus en tout ; la PR fusionnée, seulement ensuite revenir en SCOPING, qui efface l'autorisation — jamais avant : une CI rouge ne pourrait plus rouvrir le chantier. CI rouge avant la fusion : rester en CLOSING et rouvrir le chantier (`node .githooks/state.mjs reopen`, qui annule la fusion programmée). Délai dépassé, ou autre échec : rester en CLOSING et le signaler.",
    queue:
      "File décidée au lancement : ensuite {0}{1}. Avec Claude Code, chaque chantier de la file se mène dans un sous-agent (contexte neuf), un seul à la fois, dans le dossier du dépôt et jamais dans une copie isolée (worktree : l'état n'y est pas) : sa consigne commence par lire l'état (`node .githooks/state.mjs`), il mène le chantier jusqu'au retour en SCOPING sans lancer la suivante et rend un compte rendu ; la session principale ne garde que leurs comptes rendus (autre outil : tout dans la même session). Après la clôture, la session principale suit la PR par `gh` jusqu'à la fusion, 30 minutes au plus, met à jour la branche principale et lance la suivante : `node .githooks/state.mjs next`, dans un nouveau sous-agent ; plus de suivante : un bilan. S'arrêter au premier chantier qui bute (attestation humaine, FAIL non corrigé, CI rouge, délai de fusion dépassé, décision à prendre) en disant pourquoi et ce qui est attendu ; jamais de suivante sur une fusion non faite. Un compte rendu court à chaque clôture, un bilan en fin de file.",
    parallel:
      "File en parallèle décidée au lancement : {0}, chacune lancée dans son worktree par le hook (la session principale n'en crée aucun). Avec Claude Code, chaque chantier se mène dans un sous-agent (contexte neuf) dont le dossier de travail est son worktree (chemin listé ci-dessous, jamais `isolation: \"worktree\"`) : sa consigne commence par lire l'état dans ce dossier (`node <worktree>/.githooks/state.mjs`), il n'en modifie aucun autre, mène le chantier jusqu'à la clôture et rend un compte rendu ; la session principale ne garde que leurs comptes rendus (autre outil : pas de file en parallèle). La session principale suit chaque PR par `gh` jusqu'à la fusion, 30 minutes au plus chacune, sans attendre la fusion d'un autre chantier ; en cas de conflit de clôture sur l'index ou le journal des chantiers, le sous-agent rebase et résout seul. Un chantier qui demande un humain (attestation, FAIL non corrigé, CI rouge, délai de fusion dépassé, décision à prendre) arrête son sous-agent en disant pourquoi et ce qui est attendu ; les autres continuent et la session principale ne lance rien de plus d'elle-même. Un compte rendu court à chaque clôture, un bilan de file en fin : l'état de chaque chantier (worktree, branche, fiche, activité, PR).",
    queueMerge: " (fusion automatique)",
    detour:
      "Détour en cours (QUA-050), ouvert depuis {0} (branche {1}, travail en cours mis de côté par l'outil) : corriger le défaut sous cette fiche courte, sur la branche {2} ; PR intitulée « Détour de {0} : … », au mode de fusion décidé au lancement ; avant le push, entrée de journal citant la branche {2} et fiche supprimée ; une fois la PR fusionnée, `node .githooks/detour.mjs resume`. Jamais de stash ni de changement de branche à la main.",
    reopened:
      "Chantier rouvert après sa clôture (QUA-116), PR #{0} ouverte : corriger le défaut sur cette branche, sans réécrire l'historique ni pousser en force ; un correctif qui change les règles du jeu (contrat existant modifié ou retiré, hook, seuil, barrière, CI) attend le oui de l'humain (`node .githooks/detour.mjs allow`, terminal ; en file, le sous-agent s'arrête) ; puis EVIDENCE, VERIFY et CLOSING comme la première fois : l'entrée de journal du chantier se met à jour (jamais une seconde), `node .githooks/close.mjs` réécrit la preuve et remplace la ligne de consommation.",
    // Un « continue » ambigu a lancé un chantier le 2026-10-17 : une file reprise exige un oui explicite.
    queueResume:
      "Reprise de file : si cette session n'a pas mené la clôture précédente (nouvelle session, après `/clear` ou un redémarrage), demander d'abord à l'humain « la file reprend avec {0} : je lance ? » et ne lancer `node .githooks/state.mjs next` que sur un oui explicite ; sans oui explicite, rien n'est lancé.",
    closingNoQueue:
      "Hors file : terminer en proposant à l'humain `/clear` avant sa prochaine demande (Claude Code ; autre outil : une nouvelle conversation) — un contexte neuf ne relit pas ce chantier.",
    nextSession:
      "Après une clôture, le chantier suivant se lance dans une nouvelle session (Claude Code : `/clear` ; autre outil : une nouvelle conversation) — un contexte neuf ne relit pas le chantier clos ; si cette session vient d'en clore un, le proposer à l'humain avant sa prochaine demande. Une file garde son sous-agent par chantier.",
    scope: "Périmètre autorisé (bloc scope de la fiche) :",
    noScope: "  (aucun bloc scope : aucun fichier de code autorisé)",
    briefDeleted: "fiche supprimée (clôture)",
    rule: (r) => `Règle : ${r}`,
    docLanguage: (l) =>
      `Langue de la carte, du README et du changelog : ${{ fr: "français", en: "anglais" }[l] ?? l} (réglage docLang, à défaut la langue du projet).`,
    reminder: (a, b, n) =>
      `[drwil] Rappel : ${a}${b ? ` · ${b}` : " · aucune attente active"}${n === null ? "" : ` · périmètre : ${n} fichier(s) ou motif(s)`} (relire avec \`node .githooks/state.mjs\`).`,
    invalidReminder: "[drwil] Rappel : état invalide, traité comme SCOPING neutre (détail : `node .githooks/state.mjs`).",
    footer:
      "Ne jamais écrire .drwil/state.json à la main. Lancer un chantier : `/drwil-start` (sondage) ou `node .githooks/state.mjs start` dans un terminal — décision humaine, sauf réglage « transitions » : agent. Changer d'activité : `node .githooks/state.mjs move <ACTIVITE>` ; clore est libre sur évidence de verify valide. Noter une idée sans changer d'activité : `node .githooks/note.mjs '<texte>'` (une note est une intention, jamais un ordre de coder).",
    rules: {
      SCOPING: "lire, analyser, cadrer ; ne modifier aucun fichier de code.",
      EXPECTATION: "formaliser l'attente dans sa fiche ; ne modifier aucun fichier de code.",
      REQUEST: "expliciter la demande de réalisation ; ne modifier encore aucun fichier de code.",
      BUILD:
        "réaliser l'attente, en ne modifiant que les fichiers du périmètre (bloc scope de la fiche active). Un défaut préalable hors du cadrage : un détour, `node .githooks/detour.mjs open <nom> --title <défaut>`, jamais un élargissement sans rapport.",
      EVIDENCE: "produire les preuves et lancer les contrôles ; un correctif demande de revenir en BUILD.",
      VERIFY:
        "lancer `npx drwil verify --evidence` ; seul son verdict compte, pas l'affirmation de l'agent. PASS ou ATTESTED : clore ; FAIL ou ERROR : revenir en BUILD ; MANUAL : attestation humaine (`npx drwil attest`), puis relancer.",
      CLOSING:
        "fiche entièrement terminée : la condenser dans le journal, puis `node .githooks/close.mjs` la supprime, retire sa ligne de l'index, consigne la consommation et signale ce qui ferait échouer la clôture (incohérences de documentation comprises ; `--fix` applique les corrections proposées), avant le commit ; sinon mettre à jour statut et reprise. Boucler les intentions : chaque limite non traitée devient une fiche d'intention (jamais une fiche de chantier), listée dans l'index en [décision] et citée par « Limites connues » du journal ; l'intention d'origine (champ Intention de la fiche) est mise à jour, ou résumée au journal et retirée si elle est entièrement couverte. Tenir ce que lit l'utilisateur : README et ligne « Unreleased » du changelog si le chantier change ce qu'il voit, carte docs/architecture.md s'il change la structure. Consigner la consommation du chantier : `node .githooks/consumption.mjs --record <fiche>` (fait par close.mjs ; journaux de Claude Code ; autre outil : estimer à la main), la ligne entre dans le commit de clôture. Puis revenir en SCOPING. Un défaut révélé par la PR, ouverte et pas encore fusionnée (CI rouge, analyse de la forge) : `node .githooks/state.mjs reopen` rouvre le chantier sur sa branche (fusion automatique annulée d'abord, fiche restaurée, BUILD), jamais un contournement.",
    },
  },
  en: {
    header: "[drwil] Governance state, read from disk (.drwil/state.json) — do not infer it from the conversation.",
    missing: "(.drwil/state.json missing: neutral state)",
    disabled: '[drwil] Governance disabled ("scopeGate" setting: off in .drwil/ia-first.json): no activity or scope to follow.',
    invalid: "⚠ Invalid state, treated as neutral SCOPING:",
    activity: (a) => `Activity: ${a}`,
    brief: (b) => `Active brief: ${b ?? "none"}`,
    request: (r) => `Active request: ${r ?? "none"}`,
    autoMerge:
      "Automatic merge authorised at start: in CLOSING, after verify PASS and pushing the closing commit, in this order: wait until GitHub has computed the PR (`gh pr view --json mergeStateStatus`) and, if it is behind the main branch (BEHIND), update it; schedule `gh pr merge --auto --merge`; check that the merge is actually scheduled (`gh pr view --json autoMergeRequest`); follow the PR with `node .githooks/pr-watch.mjs <number>`, run again while it says « green » without « merged », 30 minutes at most in all; the PR merged, only then go back to SCOPING, which clears the authorisation — never before: a red CI could no longer reopen the workstream. Red CI before the merge: stay in CLOSING and reopen the workstream (`node .githooks/state.mjs reopen`, which cancels the scheduled merge). Time limit exceeded, or any other failure: stay in CLOSING and report it.",
    queue:
      "Queue decided at start: then {0}{1}. With Claude Code, each workstream of the queue runs in a subagent (fresh context), one at a time, in the repository folder and never in an isolated copy (worktree: the state is not there): its instructions start by reading the state (`node .githooks/state.mjs`), it carries the workstream until back in SCOPING without starting the next one and returns a report; the main session keeps only their reports (another tool: everything in the same session). After the closing, the main session follows the PR with `gh` until it is merged, 30 minutes at most, updates the main branch and starts the next one: `node .githooks/state.mjs next`, in a new subagent; no next one: a summary. Stop at the first workstream that gets stuck (human attestation, FAIL not fixed, red CI, merge delay exceeded, decision to make), saying why and what is expected; never a next one on a merge not done. A short report at each closing, a summary at the end of the queue.",
    parallel:
      "Parallel queue decided at start: {0}, each launched in its own worktree by the hook (the main session creates none). With Claude Code, each workstream runs in a subagent (fresh context) whose working folder is its worktree (path listed below, never `isolation: \"worktree\"`): its instructions start by reading the state in that folder (`node <worktree>/.githooks/state.mjs`), it modifies no other folder, carries the workstream to its closing and returns a report; the main session keeps only their reports (another tool: no parallel queue). The main session follows each PR with `gh` until it is merged, 30 minutes at most each, without waiting for another workstream's merge; on a closing conflict on the workstreams' index or journal, the subagent rebases and resolves it alone. A workstream that needs a human (attestation, FAIL not fixed, red CI, merge delay exceeded, decision to make) stops its own subagent, saying why and what is expected; the others go on and the main session starts nothing more by itself. A short report at each closing, a queue summary at the end: the state of each workstream (worktree, branch, brief, activity, PR).",
    queueMerge: " (automatic merge)",
    detour:
      "Detour in progress (QUA-050), opened from {0} (branch {1}, work in progress set aside by the tool): fix the defect under this short brief, on branch {2}; PR titled « Detour of {0}: … », with the merge mode decided at start; before the push, a journal entry citing branch {2} and the brief deleted; once the PR is merged, `node .githooks/detour.mjs resume`. Never stash or switch branches by hand.",
    reopened:
      "Workstream reopened after its closing (QUA-116), PR #{0} open: fix the defect on this branch, without rewriting history or force-pushing; a fix that changes the rules (an existing contract modified or removed, hook, threshold, gate, CI) waits for the human's yes (`node .githooks/detour.mjs allow`, terminal; in a queue, the subagent stops); then EVIDENCE, VERIFY and CLOSING like the first time: the workstream's journal entry is updated (never a second one), `node .githooks/close.mjs` rewrites the proof and replaces the consumption line.",
    queueResume:
      "Resuming a queue: if this session did not carry the previous closing (new session, after `/clear` or a restart), first ask the human « the queue resumes with {0}: shall I start it? » and run `node .githooks/state.mjs next` only on an explicit yes; without an explicit yes, nothing is started.",
    closingNoQueue:
      "Outside a queue: finish by suggesting `/clear` to the human before their next request (Claude Code; another tool: a new conversation) — a fresh context does not read this workstream again.",
    nextSession:
      "After a closing, the next workstream starts in a new session (Claude Code: `/clear`; another tool: a new conversation) — a fresh context does not read the closed workstream again; if this session just closed one, suggest it to the human before their next request. A queue keeps its subagent per workstream.",
    scope: "Allowed scope (scope block of the brief):",
    noScope: "  (no scope block: no code file allowed)",
    briefDeleted: "brief deleted (closing)",
    rule: (r) => `Rule: ${r}`,
    docLanguage: (l) =>
      `Language of the map, README and changelog: ${{ fr: "French", en: "English" }[l] ?? l} (docLang setting, else the project language).`,
    reminder: (a, b, n) =>
      `[drwil] Reminder: ${a}${b ? ` · ${b}` : " · no active brief"}${n === null ? "" : ` · scope: ${n} file(s) or pattern(s)`} (read again with \`node .githooks/state.mjs\`).`,
    invalidReminder: "[drwil] Reminder: invalid state, treated as neutral SCOPING (details: `node .githooks/state.mjs`).",
    footer:
      'Never write .drwil/state.json by hand. Start a workstream: `/drwil-start` (poll) or `node .githooks/state.mjs start` in a terminal — a human decision, unless the "transitions" setting is agent. Change activity: `node .githooks/state.mjs move <ACTIVITY>`; closing is free on valid verify evidence. Note an idea without changing activity: `node .githooks/note.mjs "<text>"` (a note is an intent, never an order to code).',
    rules: {
      SCOPING: "read, analyse, frame; do not modify any code file.",
      EXPECTATION: "write down the expectation in its brief; do not modify any code file.",
      REQUEST: "make the implementation request explicit; do not modify any code file yet.",
      BUILD:
        "implement the expectation, modifying only the files in the scope (scope block of the active brief). A prior defect outside the scope: a detour, `node .githooks/detour.mjs open <name> --title <defect>`, never an unrelated widening.",
      EVIDENCE: "produce evidence and run the checks; a fix means going back to BUILD.",
      VERIFY:
        "run `npx drwil verify --evidence`; only its verdict counts, not the agent's claim. PASS or ATTESTED: close; FAIL or ERROR: go back to BUILD; MANUAL: human attestation (`npx drwil attest`), then run it again.",
      CLOSING:
        "brief fully finished: condense it into the journal, then `node .githooks/close.mjs` deletes it, removes its index line, records the consumption and reports what would make the closing fail (documentation inconsistencies included; `--fix` applies the proposed corrections), before the commit; otherwise update status and hand-off. Close the intent loop: each limit left untreated becomes an intent brief (never a workstream brief), listed in the index as [decision] and cited by the journal's « Known limits »; the original intent (the brief's Intent field) is updated, or summarised in the journal and removed when fully covered. Keep what the user reads current: README and « Unreleased » changelog line if the workstream changes what they see, map docs/architecture.md if it changes the structure. Record the workstream's consumption: `node .githooks/consumption.mjs --record <brief>` (done by close.mjs; Claude Code logs; another tool: estimate by hand), the line goes into the closing commit. Then go back to SCOPING. A defect revealed by the PR, open and not merged yet (red CI, forge analysis): `node .githooks/state.mjs reopen` reopens the workstream on its branch (automatic merge cancelled first, brief restored, BUILD), never a workaround.",
    },
  },
};

/** Fills the `{0}`, `{1}`… holes of a message. */
export const fill = (message, ...values) => message.replace(/\{(\d)\}/g, (_, i) => values[i]);

const RERUN = { fr: "relancer `npx drwil verify --evidence`", en: "run `npx drwil verify --evidence` again" };

/** Messages of the transitions and of the state's validation (state.mjs), in the project's language. */
export const STATE_MESSAGES = {
  fr: {
    or: " ou ",
    notObject: "state.json doit contenir un objet JSON",
    unknownField: `champ inconnu « {0} » (l'état ne contient que des références)`,
    version: "version : 1 attendu",
    unknownActivity: `activité inconnue « {0} » (attendu : {1})`,
    scopingEmpty: "en SCOPING, aucune attente ni demande active",
    briefRequired: `{0} : attente active requise`,
    request: "active_request : chaîne ou null attendu",
    autoMergeType: "auto_merge : booléen attendu",
    autoMergeScoping: "auto_merge : jamais en SCOPING (aucun chantier en cours)",
    queueType: "queue : liste de fiches attendue",
    queueMergeType: "queue_merge : booléen attendu",
    parallelType: "parallel : liste de fiches attendue",
    detourType: "detour : objet { origin_brief, origin_branch, branch, stash, human? } attendu",
    detourActivity: "detour : seulement en BUILD",
    detourMove: "détour en cours : le reprendre d'abord (`node .githooks/detour.mjs resume`), ou l'abandonner en SCOPING",
    reopenedType: "reopened : objet { pr, human? } attendu (pr : numéro de la PR)",
    reopenedActivity: "reopened : seulement de BUILD à CLOSING",
    reopenFrom:
      "réouverture refusée : activité {0} — on rouvre depuis CLOSING un chantier clos dont la PR, ouverte, révèle un défaut ; rien n'a changé",
    reopenDirty: "réouverture refusée : arbre modifié — commiter ou retirer d'abord ; rien n'a changé",
    reopenBranch:
      "réouverture refusée : il faut être sur la branche du chantier (ni la branche principale, ni un HEAD détaché) ; rien n'a changé",
    reopenUnknown:
      "réouverture refusée : l'état de la PR de {0} est inconnu (gh absent, non connecté, ou aucune PR) — un état non établi ne rouvre rien ; rien n'a changé",
    reopenNotOpen:
      "réouverture refusée : la PR #{0} est {1}, pas ouverte — un défaut après la fusion relève d'un nouveau chantier ou de l'entretien courant ; rien n'a changé",
    reopenNoBrief: "réouverture refusée : aucun commit de {1} ne supprime {0} — rien à restaurer ; rien n'a changé",
    reopenAutoMerge:
      "réouverture refusée : la fusion automatique de la PR #{0} n'a pas pu être annulée (`gh pr merge {0} --disable-auto`) — la branche ne reçoit rien tant qu'elle est programmée ; rien d'autre n'a changé",
    reopenCancelled: "fusion automatique annulée : à reprogrammer après la nouvelle clôture, comme la première fois",
    reopenMessage: "Réouverture de {0} : défaut révélé par la PR #{1}",
    reopenCommitFailed: "réouverture refusée : le commit de la fiche restaurée a échoué ({0}) — fiche retirée, état inchangé",
    reopenRestored:
      "fiche {0} restaurée telle qu'avant la clôture, dans son propre commit ({1}) ; son cadrage s'élargit comme d'habitude, dans un commit séparé",
    reopened: "CLOSING → BUILD (réouverture de {0}, PR #{1} ouverte)",
    queueNotReady:
      "lancement refusé : « {0} » ne peut pas entrer dans une file — il faut une fiche prête (maturité « prêt », aucune [décision] ouverte, critères d'acceptation)",
    queued: "file : ensuite {0}, lancés un à un par l'agent après chaque clôture et fusion (arrêt : `node .githooks/state.mjs queue stop`)",
    noQueue: "aucune file en cours",
    queueStopped: "file arrêtée : {0} ne seront pas lancés ; le chantier en cours continue",
    since: "since : horodatage ISO 8601 ou null attendu",
    briefType: "active_brief : chemin de fiche ou null attendu",
    briefPlace: `active_brief : fiche .md de {1} attendue (« {0} »)`,
    briefModel: `active_brief : « {0} » est un modèle, pas une fiche de chantier`,
    briefMissing: `active_brief : fiche « {0} » introuvable`,
    unreadable: "state.json illisible (JSON invalide)",
    noEvidence: "aucune évidence de verify : lancer `npx drwil verify --evidence`",
    evidenceUnreadable: `évidence {0} illisible : ${RERUN.fr}`,
    noHead: "commit HEAD introuvable : la clôture se juge sur un travail commité",
    otherCommit: `évidence {0} produite sur un autre commit que HEAD : ${RERUN.fr}`,
    dirty: `arbre modifié (au moment de verify ou depuis) : commiter puis ${RERUN.fr}`,
    manual: `verdict MANUAL : un humain atteste (\`npx drwil attest <ID>\`), puis ${RERUN.fr}`,
    failed: `verdict {0} : revenir en BUILD (\`node .githooks/state.mjs move BUILD\`) pour corriger`,
    criteria: `critère(s) d'acceptation non prouvé(s) : {0} — lier chacun à un contrat avec contrôle (ou un contrat Manuel attesté), puis ${RERUN.fr}`,
    noVerdict: `évidence {0} sans verdict reconnu : ${RERUN.fr}`,
    undeclared: `contrat(s) modifié(s) sans déclaration : {0} — régression, ou évolution à déclarer dans la section « Contrats modifiés » de la fiche (validée par le demandeur), puis ${RERUN.fr}`,
    noCriteria:
      "avertissement : la fiche « {0} » n'a aucun critère d'acceptation — ce chantier ne laisse aucun test de non-régression aux suivants (section « Critères d'acceptation » du modèle de fiche)",
    startFrom: `lancement refusé : activité {0} (lancer se fait depuis SCOPING, EXPECTATION ou REQUEST, ou CLOSING une fois la PR de clôture fusionnée)`,
    startClosingBrief: "lancement refusé : CLOSING et la fiche {0} est encore là — la clôturer d'abord (recette « Clôturer »)",
    startClosingOpen:
      "lancement refusé : CLOSING, la PR de clôture n'est pas fusionnée — attendre la fusion (et mettre la branche principale à jour), puis relancer",
    startClosingUnknown: "lancement refusé : CLOSING, la fusion de la PR de clôture n'est pas établie",
    startHuman: "lancement refusé : c'est une décision humaine (sondage `/drwil-start`, ou terminal interactif)",
    startBusy: "lancement refusé : la fiche {0} est déjà active dans le worktree {1}",
    startNotScoped: `lancement refusé : « {0} » n'est pas une fiche cadrée (bloc scope commité, fiche non terminée)`,
    startNoCriteria:
      "lancement refusé : la fiche « {0} » n'a aucun critère d'acceptation — chacun, prouvé par un contrat, devient la non-régression des chantiers suivants (section « Critères d'acceptation » du modèle de fiche)",
    closingNoCriteria:
      "clôture refusée : la fiche « {0} » n'a aucun critère d'acceptation — en écrire, chacun prouvé par un contrat, puis relancer `drwil verify`",
    startInvalid: `lancement refusé : {0}`,
    started: `{0} → BUILD (lancé : {1}){2}`,
    mergeAuthorised: " — fusion automatique autorisée",
    forbidden: `transition {0} → {1} interdite (depuis {0} : {2})`,
    briefOption: "--brief ne sert qu'à ouvrir une attente (SCOPING → EXPECTATION)",
    briefNeeded: "ouvrir une attente exige --brief <docs/projects/….md>",
    resumed: `état précédent invalide, repris depuis le neutre ({0}) ; `,
    closingRefused: `clôture refusée : {0}`,
    closingSignals: "Ce qui ferait échouer la clôture, à corriger tant que le code peut changer (`node .githooks/state.mjs move BUILD`) :",
    stillThere: `retour en SCOPING refusé : {0} se dit terminée mais est encore là — la condenser dans docs/projects/journal.md, la supprimer et retirer sa ligne de l'index (recette « Clôturer »)`,
    humanDecision: `{0} → {1} est une décision humaine : terminal interactif requis (réglage « transitions » : human)`,
    confirm: `Pour passer de {0} à {1}, retapez l'activité visée ({1}) : `,
    confirmRefused: "confirmation refusée : état inchangé",
    noBriefs: "(aucune fiche cadrée : bloc scope commité et fiche non terminée)",
    notReady: ` · non prête : {0} décision(s) à trancher`,
    refine: " · à affiner : à revoir avant de la lancer",
    pick: "Numéro de la fiche à lancer : ",
    confirmStart: `Lancer {0} ? Retapez START : `,
    askMerge: "Fusion automatique à la fin du chantier ? Retapez MERGE, sinon Entrée : ",
    notConfirmed: "✗ lancement non confirmé : état inchangé",
    unreadableState: `[drwil] état de gouvernance illisible : {0}`,
    usage: `usage : node .githooks/state.mjs move <{0}> [--brief <docs/projects/….md>] [--request <texte>] | start [<fiche>] | reopen | briefs [--json] | reminder`,
  },
  en: {
    or: " or ",
    notObject: "state.json must hold a JSON object",
    unknownField: `unknown field "{0}" (the state only holds references)`,
    version: "version: 1 expected",
    unknownActivity: `unknown activity "{0}" (expected: {1})`,
    scopingEmpty: "in SCOPING, no active brief nor request",
    briefRequired: `{0}: active brief required`,
    request: "active_request: string or null expected",
    autoMergeType: "auto_merge: boolean expected",
    autoMergeScoping: "auto_merge: never in SCOPING (no workstream in progress)",
    queueType: "queue: list of briefs expected",
    queueMergeType: "queue_merge: boolean expected",
    parallelType: "parallel: list of briefs expected",
    detourType: "detour: object { origin_brief, origin_branch, branch, stash, human? } expected",
    detourActivity: "detour: only in BUILD",
    detourMove: "detour in progress: resume it first (`node .githooks/detour.mjs resume`), or abandon it to SCOPING",
    reopenedType: "reopened: object { pr, human? } expected (pr: the PR's number)",
    reopenedActivity: "reopened: only from BUILD to CLOSING",
    reopenFrom:
      "reopening refused: activity {0} — a closed workstream whose open PR reveals a defect is reopened from CLOSING; nothing changed",
    reopenDirty: "reopening refused: worktree modified — commit or remove first; nothing changed",
    reopenBranch: "reopening refused: it takes the workstream's branch (neither the main branch nor a detached HEAD); nothing changed",
    reopenUnknown:
      "reopening refused: the state of the PR of {0} is unknown (gh missing, not logged in, or no PR) — a state not established reopens nothing; nothing changed",
    reopenNotOpen:
      "reopening refused: PR #{0} is {1}, not open — a defect after the merge belongs to a new workstream or to routine maintenance; nothing changed",
    reopenNoBrief: "reopening refused: no commit of {1} deletes {0} — nothing to restore; nothing changed",
    reopenAutoMerge:
      "reopening refused: the automatic merge of PR #{0} could not be cancelled (`gh pr merge {0} --disable-auto`) — the branch receives nothing while it is scheduled; nothing else changed",
    reopenCancelled: "automatic merge cancelled: schedule it again after the new closing, like the first time",
    reopenMessage: "Reopening of {0}: defect revealed by PR #{1}",
    reopenCommitFailed: "reopening refused: the commit of the restored brief failed ({0}) — brief removed, state unchanged",
    reopenRestored:
      "brief {0} restored as it was before the closing, in a commit of its own ({1}); its scope widens as usual, in a separate commit",
    reopened: "CLOSING → BUILD (reopening of {0}, PR #{1} open)",
    queueNotReady:
      'start refused: "{0}" cannot enter a queue — it takes a ready brief (maturity « ready », no open [decision], acceptance criteria)',
    queued: "queue: then {0}, started one by one by the agent after each closing and merge (stop: `node .githooks/state.mjs queue stop`)",
    noQueue: "no queue in progress",
    queueStopped: "queue stopped: {0} will not be started; the current workstream goes on",
    since: "since: ISO 8601 timestamp or null expected",
    briefType: "active_brief: brief path or null expected",
    briefPlace: `active_brief: .md brief of {1} expected ("{0}")`,
    briefModel: `active_brief: "{0}" is a model, not a workstream brief`,
    briefMissing: `active_brief: brief "{0}" not found`,
    unreadable: "state.json unreadable (invalid JSON)",
    noEvidence: "no verify evidence: run `npx drwil verify --evidence`",
    evidenceUnreadable: `evidence {0} unreadable: ${RERUN.en}`,
    noHead: "HEAD commit not found: closing is judged on committed work",
    otherCommit: `evidence {0} produced on another commit than HEAD: ${RERUN.en}`,
    dirty: `worktree modified (at verify time or since): commit, then ${RERUN.en}`,
    manual: `MANUAL verdict: a human attests (\`npx drwil attest <ID>\`), then ${RERUN.en}`,
    failed: `{0} verdict: go back to BUILD (\`node .githooks/state.mjs move BUILD\`) to fix`,
    criteria: `unproven acceptance criteria: {0} — link each to a contract with a check (or an attested Manual contract), then ${RERUN.en}`,
    noVerdict: `evidence {0} without a known verdict: ${RERUN.en}`,
    undeclared: `contract(s) modified without declaration: {0} — a regression, or a change to declare in the « Modified contracts » section of the brief (approved by the requester), then ${RERUN.en}`,
    noCriteria:
      'warning: brief "{0}" has no acceptance criterion — this workstream leaves no non-regression test for the next ones ("Acceptance criteria" section of the brief model)',
    startFrom: `start refused: activity {0} (start from SCOPING, EXPECTATION or REQUEST, or CLOSING once the closing PR is merged)`,
    startClosingBrief: 'start refused: CLOSING and brief {0} is still there — close it first ("Closing" recipe)',
    startClosingOpen:
      "start refused: CLOSING, the closing PR is not merged — wait for the merge (and update the main branch), then start again",
    startClosingUnknown: "start refused: CLOSING, the closing PR's merge is not established",
    startHuman: "start refused: it is a human decision (`/drwil-start` poll, or interactive terminal)",
    startBusy: "start refused: brief {0} is already active in the worktree {1}",
    startNotScoped: `start refused: "{0}" is not a scoped brief (scope block committed, brief not finished)`,
    startNoCriteria:
      'start refused: brief "{0}" has no acceptance criterion — each one, proven by a contract, becomes the non-regression of the next workstreams ("Acceptance criteria" section of the brief model)',
    closingNoCriteria:
      'closing refused: brief "{0}" has no acceptance criterion — write some, each proven by a contract, then run `drwil verify` again',
    startInvalid: `start refused: {0}`,
    started: `{0} → BUILD (started: {1}){2}`,
    mergeAuthorised: " — automatic merge authorised",
    forbidden: `transition {0} → {1} forbidden (from {0}: {2})`,
    briefOption: "--brief only opens an expectation (SCOPING → EXPECTATION)",
    briefNeeded: "opening an expectation requires --brief <docs/projects/….md>",
    resumed: `previous state invalid, resumed from neutral ({0}); `,
    closingRefused: `closing refused: {0}`,
    closingSignals: "What would make the closing fail, to fix while code can still change (`node .githooks/state.mjs move BUILD`):",
    stillThere: `back to SCOPING refused: {0} says it is done but is still there — condense it into docs/projects/journal.md, delete it and remove its index line ("Closing" recipe)`,
    humanDecision: `{0} → {1} is a human decision: interactive terminal required ("transitions" setting: human)`,
    confirm: `To move from {0} to {1}, type the target activity again ({1}): `,
    confirmRefused: "confirmation refused: state unchanged",
    noBriefs: "(no scoped brief: scope block committed and brief not finished)",
    notReady: ` · not ready: {0} decision(s) to make`,
    refine: " · to refine: review before starting it",
    pick: "Number of the brief to start: ",
    confirmStart: `Start {0}? Type START: `,
    askMerge: "Automatic merge at the end of the workstream? Type MERGE, otherwise Enter: ",
    notConfirmed: "✗ start not confirmed: state unchanged",
    unreadableState: `[drwil] governance state unreadable: {0}`,
    usage: `usage: node .githooks/state.mjs move <{0}> [--brief <docs/projects/….md>] [--request <text>] | start [<brief>] | reopen | briefs [--json] | reminder`,
  },
};

const language = (root) => (config(root).lang === "en" ? "en" : "fr");

/** Language of what a visitor reads (map, README, changelog): `docLang`, else the project's (QUA-040). */
export const docLanguage = (root) => config(root).docLang ?? config(root).lang ?? "fr";

/** `scopeGate: off` switches off all governance (gate, trailer, context): withdrawal without deleting anything. */
export const governanceDisabled = (root) => config(root).scopeGate === "off";

/** Readable context to reinject to the agent, built from the disk only. */
export function context(read, root) {
  const T = TEXTS[language(root)];
  if (governanceDisabled(root)) return T.disabled;
  const { state, source, problems } = read;
  const lines = [T.header];
  if (source === "missing") lines.push(T.missing);
  if (problems.length) lines.push(T.invalid, ...problems.map((p) => `  - ${p}`));
  lines.push(T.activity(state.activity));
  let brief = state.active_brief;
  let patterns = null;
  if (brief && !existsSync(join(root, brief))) {
    brief = `${brief} — ${T.briefDeleted}`;
  } else if (brief) {
    try {
      const text = readFileSync(join(root, brief), "utf8");
      const title = /^#\s+(.+)$/m.exec(text)?.[1]?.trim();
      if (title) brief = `${brief} — ${title}`;
      patterns = readBlock(text).patterns;
    } catch {
      patterns = [];
    }
  }
  lines.push(T.brief(brief), T.request(state.active_request));
  if (state.auto_merge === true) lines.push(T.autoMerge);
  if (state.queue?.length) lines.push(fill(T.queue, state.queue.join(", "), state.queue_merge ? T.queueMerge : ""));
  if (state.parallel?.length) lines.push(fill(T.parallel, state.parallel.join(", ")));
  if (state.queue?.length && state.activity === "SCOPING") lines.push(fill(T.queueResume, state.queue[0]));
  if (state.detour) lines.push(fill(T.detour, state.detour.origin_brief, state.detour.origin_branch, state.detour.branch));
  if (state.reopened) lines.push(fill(T.reopened, state.reopened.pr));
  if (patterns) lines.push(T.scope, ...(patterns.length ? patterns.map((p) => `  - ${p}`) : [T.noScope]));
  lines.push(T.rule(T.rules[state.activity]));
  if (state.activity === "CLOSING") lines.push(T.docLanguage(docLanguage(root)));
  if (state.activity === "CLOSING" && !state.queue?.length) lines.push(T.closingNoQueue);
  // Back in SCOPING by a move (`since` set; a missing state has none), outside a queue: one session per workstream (QUA-131).
  if (state.activity === "SCOPING" && state.since && !state.queue?.length) lines.push(T.nextSession);
  lines.push(...worktreeLines(root));
  lines.push(T.footer);
  return lines.join("\n");
}

/** Short reminder (two lines) of the state, to inject with each message; null if governance is disabled. */
export function shortReminder(read, root) {
  const T = TEXTS[language(root)];
  if (governanceDisabled(root)) return null;
  const { state, problems } = read;
  const head = problems.length
    ? T.invalidReminder
    : (() => {
        let n = null;
        if (state.active_brief && !existsSync(join(root, state.active_brief))) {
          return T.reminder(state.activity, `${state.active_brief} (${T.briefDeleted})`, null);
        }
        if (state.active_brief) {
          try {
            n = readBlock(readFileSync(join(root, state.active_brief), "utf8")).patterns.length;
          } catch {
            n = 0;
          }
        }
        return T.reminder(state.activity, state.active_brief, n);
      })();
  return `${head}\n${T.rule(T.rules[state.activity])}`;
}
