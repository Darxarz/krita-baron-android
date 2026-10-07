/**
 * promptOrder — «разложить всё по местам»: порядок фрагментов промпта под
 * конкретную модель, БЕЗ потери и замены слов.
 *
 * Что делает модуль.
 *  1. classifyPromptSegment — детерминированно относит фрагмент («пузырь» из
 *     promptSegments) к смысловой группе: качество, количество персонажей,
 *     персонаж, серия, художник, внешность, одежда, эмоция, поза, место,
 *     камера, свет, стиль… Источники: словарь тегов (категории danbooru:
 *     character / copyright / artist / meta) и курируемые списки слов ниже.
 *  2. organizePrompt — переставляет фрагменты в рекомендуемом для семейства
 *     порядке. Меняется ТОЛЬКО порядок: разделители (запятые, переводы строк)
 *     остаются на своих местах, веса (tag:1.2), <lora:…>, BREAK и прочий
 *     синтаксис A1111 — неделимые единицы. BREAK/AND делят промпт на блоки,
 *     и фрагменты не перепрыгивают через них. Удаление дублей — отдельная
 *     опция, по умолчанию выключена («без потери слов»).
 *  3. insertTagLogically / logicalMoveTarget / categoryStepTarget — куда
 *     вставить новый тег (подсказки Каданса, триггеры LoRA) и куда сдвинуть
 *     один фрагмент — по тому же классификатору.
 *
 * Порядок по семействам (см. ORDERS; источники — карточки моделей):
 *  - Illustrious / NoobAI / SDXL-теги: качество → 1girl/1boy/solo →
 *    персонаж → серия → художник → остальные теги (внешность → одежда →
 *    эмоция → поза → взаимодействие → место → камера → свет) → стиль/мета.
 *    NoobAI-XL (huggingface.co/Laxhar/noobai-XL-1.1): «<1girl/1boy/1other/…>,
 *    <character>, <series>, <artists>, <special tags>, <general tags>»,
 *    качество «masterpiece, best quality, newest, absurdres, highres» в начале.
 *    Animagine XL (huggingface.co/cagliostrolab/animagine-xl-4.0) — тот же
 *    порядок «1girl → персонаж → серия → художник → общие теги».
 *  - Pony Diffusion V6 XL (civitai.com/models/257749): сначала
 *    score_9, score_8_up, … затем source_* и rating_*, потом описание.
 *    Имена художников в Pony зашумлены при обучении — стиль и художник в
 *    конце.
 *  - Anima: «качество/мета/год/безопасность → количество → персонаж/серия/
 *    художник (@artist) → внешность → действие → окружение и свет» — тот же
 *    порядок, что в подсказке Каданса (AssistantsRail, assistants.family.anima).
 *  - Flux / Krea 2 / Z-Image (естественный язык): слова внутри предложений не
 *    трогаем. Переставляются только целые предложения (субъект → детали →
 *    действие → окружение → камера → свет → стиль); одно длинное предложение
 *    с придаточными остаётся как есть. Список коротких тегов без точек
 *    переставляется по тем же группам.
 *
 * Модуль чистый (без React и DOM): тест грузит его вместе с promptSegments.js
 * через vm (tests/promptOrder.test.js).
 */
import { segmentPrompt, normalizeKey, cleanInsert, insertAfterSegment, appendSegment, moveSegment } from './promptSegments.js';

export const PROMPT_CATEGORIES = [
  'quality', 'subject', 'character', 'copyright', 'artist', 'trigger', 'species',
  'body', 'clothing', 'expression', 'pose', 'interaction', 'setting', 'camera',
  'lighting', 'style', 'meta', 'lora', 'unknown',
];

// Порядок групп для теговых семейств. Всё, чего нет в списке, — в конец.
const ORDERS = {
  illustrious: ['quality', 'subject', 'character', 'copyright', 'artist', 'trigger', 'species',
    'body', 'clothing', 'expression', 'pose', 'interaction', 'setting', 'camera', 'lighting',
    'style', 'meta', 'lora'],
  pony: ['quality', 'subject', 'character', 'copyright', 'trigger', 'species', 'body',
    'clothing', 'expression', 'pose', 'interaction', 'setting', 'camera', 'lighting',
    'artist', 'style', 'meta', 'lora'],
  anima: ['quality', 'subject', 'character', 'copyright', 'artist', 'trigger', 'species',
    'body', 'clothing', 'expression', 'pose', 'interaction', 'setting', 'camera', 'lighting',
    'style', 'meta', 'lora'],
};

// Естественный язык: крупные группы, качество — в конце (Flux его не учит).
const NATURAL_RANK = {
  subject: 0, character: 0, copyright: 0, trigger: 0, species: 0,
  body: 1, clothing: 1, expression: 1,
  pose: 2, interaction: 2,
  setting: 3,
  camera: 4,
  lighting: 5,
  style: 6, artist: 6,
  quality: 7, meta: 7,
  lora: 8,
};

// Порядок внутри блока качества
const QUALITY_SUB = {
  pony: ['score', 'source', 'rating', 'quality', 'aesthetic', 'date', 'resolution', 'embedding'],
  default: ['quality', 'aesthetic', 'date', 'resolution', 'rating', 'score', 'source', 'embedding'],
};

export const ORDER_FAMILIES = ['illustrious', 'pony', 'anima', 'natural'];

/**
 * Семейство порядка по имени модели и семейству из utils/modelFamily.js.
 * Pony — ветка SDXL со своими score-тегами, её узнаём по имени файла.
 */
export function promptOrderFamily(modelName, familyId) {
  const name = String(modelName || '').toLowerCase();
  const fam = String(familyId || '').toLowerCase();
  if (fam === 'flux' || fam === 'krea' || fam === 'zimage') return 'natural';
  if (fam === 'anima') return 'anima';
  if (/pony|pdxl/.test(name)) return 'pony';
  return 'illustrious';
}

/* ───────────────────────────── Словари ─────────────────────────────────── */

const set = (s) => new Set(s.split('|').map((x) => x.trim()).filter(Boolean));

const QUALITY_EXACT = set('masterpiece|best quality|high quality|good quality|great quality|amazing quality|normal quality|medium quality|low quality|worst quality|bad quality|top quality|perfect quality|ultra quality|highest quality|highly detailed|ultra detailed|ultra-detailed|extremely detailed|very detailed|detailed|intricate|intricate details|hyperdetailed|hyper detailed|sharp focus|award winning|award-winning|professional|high detail|best|amazing');
const AESTHETIC_EXACT = set('very aesthetic|aesthetic|displeasing|very displeasing|very awa|worst aesthetic|best aesthetic');
const DATE_EXACT = set('newest|recent|mid|early|old|oldest');
const RESOLUTION_EXACT = set('absurdres|highres|incredibly absurdres|lowres|high resolution|ultra high res|ultra high resolution|8k|4k|16k|uhd|hdr|hd|8k uhd|4k uhd');
const RATING_EXACT = set('general|sensitive|questionable|explicit|nsfw|sfw|safe|suggestive');

const SUBJECT_EXACT = set('solo|duo|trio|group|solo focus|male focus|female focus|no humans|multiple girls|multiple boys|multiple others|couple|male|female|male/male|male/female|female/female|m/m|f/f|m/f|gynomorph|andromorph|intersex|ambiguous gender|herm|futanari|futa|crowd|solo male|solo female|male solo|female solo');
const SUBJECT_WORDS = set('man|woman|men|women|boy|girl|boys|girls|guy|guys|lady|person|people|character|warrior|knight|wizard|mage|soldier|king|queen|prince|princess|hero|heroine|adventurer|hunter|barbarian|samurai|ninja|pirate|priest|nun|maid|butler|student|teacher|nurse|doctor|chef|farmer|mercenary|gladiator|viking|monk|paladin|rogue|bard|sorcerer|sorceress|witch|necromancer|shaman|druid|ranger|assassin|thief|guard|gentleman|businessman|athlete|bodybuilder|wrestler|boxer|lifeguard|firefighter|policeman|officer|sailor|astronaut|pilot|twins|siblings|daddy|dad|father|mother|son|daughter|brothers|sisters|lovers|cowboy|cowgirl|lumberjack|biker|trucker|miner|blacksmith|scientist|detective|captain|general|commander|emperor|empress|chieftain|shogun|hunk');

const SPECIES_WORDS = set('furry|anthro|kemono|feral|humanoid|monster|beast|creature|animal|human|elf|elves|dwarf|halfling|orc|orcs|ork|goblin|hobgoblin|troll|trolls|ogre|giant|demon|devil|imp|angel|vampire|werewolf|lycan|dragon|dragons|wyvern|wolf|wolves|fox|cat|dog|lion|lioness|tiger|leopard|cheetah|panther|jaguar|lynx|bear|panda|horse|stallion|zebra|bull|cow|ox|bison|buffalo|deer|stag|reindeer|moose|elk|rabbit|bunny|hare|bat|bird|eagle|hawk|falcon|owl|raven|crow|parrot|penguin|shark|orca|whale|dolphin|fish|snake|serpent|naga|lizard|gecko|reptile|crocodile|alligator|turtle|tortoise|frog|toad|raccoon|hyena|rat|mouse|squirrel|otter|beaver|badger|skunk|weasel|ferret|pig|boar|hog|goat|sheep|ram|minotaur|centaur|satyr|faun|gryphon|griffon|kobold|gnoll|lizardfolk|lizardman|argonian|khajiit|tauren|draenei|worgen|sergal|protogen|avali|robot|android|cyborg|mecha|alien|slime|ghost|zombie|undead|skeleton|lich|mermaid|merman|canine|canid|feline|felid|equine|bovine|ursine|avian|reptilian|mammal|lupine|vulpine|dinosaur|raptor|kangaroo|koala|monkey|ape|gorilla|elephant|rhino|rhinoceros|hippo|giraffe|camel|jackal|coyote|dingo|husky|catboy|catgirl|dogboy|doggirl|wolfboy|wolfgirl|foxboy|foxgirl|kitsune|nekomimi|tanuki|oni|yokai|fairy|pixie|nymph|golem|gargoyle|insect|spider|arachnid|dragonborn|tiefling|hybrid|chimera|hellhound|cerberus|unicorn|pegasus|phoenix|hydra|pokemon|digimon|yeti|sasquatch|bigfoot');

const BODY_HEADS = set('hair|hairstyle|haircut|bangs|ponytail|twintails|braid|braids|bun|mohawk|ahoge|sidelocks|dreadlocks|afro|undercut|eyes|eye|pupils|iris|eyebrows|eyelashes|ears|ear|tail|tails|wings|wing|horns|horn|antlers|halo|fur|skin|scales|feathers|body|bodies|build|physique|figure|muscles|muscle|abs|abdomen|pecs|pectorals|chest|biceps|triceps|shoulders|arms|legs|thighs|calves|hips|waist|butt|ass|buttocks|breasts|breast|boobs|nipples|nipple|areolae|penis|cock|dick|balls|testicles|scrotum|sheath|knot|foreskin|glans|pussy|vagina|genitals|anus|crotch|bulge|belly|navel|stomach|gut|paws|paw|claws|claw|fangs|fang|teeth|tusks|tusk|beard|mustache|moustache|stubble|goatee|sideburns|freckles|moles|mole|scar|scars|tattoo|tattoos|markings|stripes|spots|makeup|lipstick|eyeshadow|eyeliner|muzzle|snout|mane|whiskers|lips|nose|cheeks|chin|jaw|neck|hand|hands|fingers|fingernails|nails|feet|foot|toes|soles|hooves|hoof|height|age|complexion|tan|tanline|tanlines|veins|sweat|armpits|armpit|heterochromia|physique|musculature|trapezius|thigh|forearms|forearm|calf|ankles');
const BODY_WORDS = set('muscular|muscle|slim|slender|skinny|thin|chubby|fat|overweight|plump|obese|curvy|voluptuous|thick|stocky|burly|bulky|beefy|brawny|buff|athletic|toned|petite|tall|short|tiny|mature|elderly|young|adult|bara|hairy|bald|hunky|ripped|shredded|lean|big|huge|large|massive|hyper|pale|dark-skinned|tanned|albino|freckled|bearded');

const CLOTHING_WORDS = set('nude|naked|topless|bottomless|barefoot|shirtless|pantsless|clothed|clothing|clothes|outfit|costume|attire|garment|underwear|lingerie|undressed|undressing|jewelry|jewellery|necklace|pendant|earrings|earring|piercing|piercings|bracelet|bracelets|ring|rings|anklet|choker|collar|leash|harness|belt|strap|straps|glasses|eyewear|sunglasses|goggles|monocle|eyepatch|mask|headphones|crown|tiara|circlet|hat|cap|beanie|helmet|hood|headband|headwear|headdress|hairband|hairclip|ornament|ribbon|bow|bowtie|necktie|tie|scarf|bandana|bandanna|gloves|glove|gauntlets|bracers|wristband|armband|sleeves|sleeve|cape|cloak|robe|robes|kimono|yukata|hakama|uniform|suit|tuxedo|vest|waistcoat|jacket|coat|trenchcoat|hoodie|sweater|cardigan|shirt|t-shirt|tshirt|blouse|top|tanktop|dress|gown|skirt|miniskirt|pants|trousers|jeans|shorts|leggings|tights|pantyhose|stockings|thighhighs|socks|kneehighs|boots|boot|shoes|shoe|sneakers|heels|sandals|slippers|loafers|panties|bra|briefs|boxers|jockstrap|thong|loincloth|fundoshi|speedo|trunks|bikini|swimsuit|swimwear|leotard|bodysuit|corset|apron|overalls|dungarees|armor|armour|pauldrons|pauldron|breastplate|chainmail|greaves|tabard|sash|backpack|bag|pouch|satchel|holster|quiver|sword|swords|katana|blade|axe|spear|lance|halberd|shield|staff|wand|scepter|arrow|arrows|crossbow|gun|guns|rifle|pistol|revolver|shotgun|weapon|weapons|dagger|knife|hammer|mace|whip|scythe|trident|book|scroll|cup|mug|bottle|phone|smartphone|umbrella|lantern|torch|guitar|microphone|cigarette|cigar|bouquet|flag|banner|chains|jersey|singlet|tunic|poncho|pajamas|bathrobe|towel|diaper|latex|leather|denim|fishnet|lace|spandex|gear|equipment|accessories|accessory|bandages|bandage|wrappings|tassels|cuffs|anklets|headgear|visor|earmuffs|bells|bell|medal|badge|epaulettes|cravat|jabot|ascot');

const EXPRESSION_EXACT = set('smile|smiling|grin|grinning|smirk|smug|frown|frowning|pout|pouting|blush|blushing|light blush|tears|crying|sobbing|laughing|laugh|open mouth|closed mouth|tongue out|tongue|licking lips|parted lips|clenched teeth|drooling|drool|saliva|ahegao|closed eyes|half-closed eyes|half closed eyes|one eye closed|wink|winking|rolling eyes|wide-eyed|empty eyes|heart-shaped pupils|looking at viewer|looking away|looking back|looking down|looking up|looking at another|looking to the side|looking at self|eye contact|expressionless|bedroom eyes|sweatdrop|heavy breathing|panting|moaning|screaming|shouting|yelling|fangs out|:d|:3|:p|;)|^_^|>_<|o_o|evil smile|evil grin|naughty face|smug face|happy face|seductive smile|gentle smile|teasing|teeth showing');
const EXPRESSION_WORDS = set('smile|smiling|grin|grinning|smirk|smirking|smug|frown|frowning|pout|blush|blushing|tears|crying|laughing|angry|anger|annoyed|happy|sad|surprised|shocked|scared|afraid|embarrassed|shy|nervous|serious|confident|determined|calm|bored|tired|sleepy|excited|aroused|horny|seductive|flirty|lustful|pleading|expression|expressions|face|gaze|stare|staring|glare|glaring|scowl|scowling|wink|mouth|emotion|mood|cheerful|joyful|grumpy|menacing|fierce|stern|playful|mischievous|sultry|dreamy|melancholic|melancholy|tearful|furious|proud|arrogant|content|relaxed');

const POSE_EXACT = set('on back|on stomach|on side|on all fours|all fours|arms up|arms behind head|arms behind back|arm up|hand on hip|hands on hips|hand on own chest|crossed arms|arms crossed|crossed legs|legs crossed|spread legs|legs apart|legs up|bent over|arched back|contrapposto|action pose|dynamic pose|fighting stance|battle stance|salute|peace sign|v sign|thumbs up|middle finger|hand up|hands up|outstretched arm|outstretched arms|outstretched hand|head tilt|hands in pockets|hand in pocket|hand on head|hand on face|hand on own face|hands together|presenting|showing off|flexing biceps|double biceps|leg up|knee up|hand on thigh|hand on own thigh|arm behind head|squatting|kneeling|standing|sitting|lying|seiza|wariza|indian style|straddling|stretching|flexing|posing|pose|walking|running|jumping|flying|floating|falling|leaning forward|leaning back|leaning|on top|girl on top|boy on top|male on top|female on top|on lap|sitting on lap|sitting on face|face down ass up|lying on back|lying on side|lying on stomach');
const POSE_WORDS = set('standing|sitting|kneeling|lying|reclining|squatting|crouching|walking|running|jumping|flying|floating|falling|leaning|bending|stretching|flexing|posing|pose|poses|dancing|fighting|punching|kicking|waving|pointing|reaching|climbing|swimming|sleeping|resting|relaxing|meditating|praying|eating|drinking|smoking|reading|writing|cooking|bathing|showering|holding|lifting|raising|raised|spread|crossed|outstretched|stance|posture|sprawled|lounging|sprinting|charging|attacking|casting|swinging|throwing|drawing|aiming|wielding|carrying|pulling|pushing|riding|stomping|tiptoes|kneel|sit|stand|crouch|squat|stride|striding|strutting|marching|hovering|perched|seated');

const INTERACTION_EXACT = set('hug|hugging|embrace|embracing|cuddling|cuddle|spooning|kiss|kissing|french kiss|holding hands|hand holding|princess carry|piggyback|arm around shoulder|arm around waist|sex|anal|oral|fellatio|blowjob|cunnilingus|rimming|rimjob|handjob|footjob|paizuri|titjob|frotting|frottage|grinding|penetration|fingering|masturbation|mutual masturbation|threesome|foursome|orgy|group sex|gangbang|doggystyle|missionary|cowgirl position|reverse cowgirl position|mating press|standing sex|69|deepthroat|irrumatio|cum|cumshot|ejaculation|orgasm|creampie|cum inside|cum on body|bukkake|facial|size difference|height difference|vore|fight|battle|duel|arm wrestling|wrestling|tickling|spanking|groping|petting|headpat|feeding|handshake|high five|fist bump|bondage|bdsm|domination|submission|licking|biting|sucking|grabbing|touching');
const INTERACTION_WORDS = set('sex|hug|hugging|kiss|kissing|cuddling|embrace|embracing|penetration|penetrating|fellatio|blowjob|handjob|footjob|masturbation|masturbating|cum|cumming|ejaculation|orgasm|threesome|orgy|gangbang|vore|groping|grabbing|licking|sucking|biting|touching|spanking|tickling|wrestling|fighting|duel|together|another|other|partner|partners|each|romance|romantic|lovers|rimming|frottage|grinding|domination|dominating|submissive|bondage|bdsm|mating|breeding|copulation');

const SETTING_WORDS = set('background|indoors|indoor|inside|outdoors|outdoor|outside|forest|woods|jungle|swamp|city|cityscape|town|village|street|alley|road|bridge|building|buildings|house|home|room|bedroom|bathroom|kitchen|classroom|school|office|library|gym|lockerroom|locker|shower|sauna|onsen|pool|beach|ocean|sea|lake|river|waterfall|water|underwater|mountain|mountains|hill|hills|cliff|cave|desert|field|meadow|grass|garden|park|castle|palace|dungeon|prison|ruins|temple|shrine|church|cathedral|tavern|inn|bar|pub|restaurant|cafe|market|shop|store|stage|arena|battlefield|colosseum|throne|space|planet|galaxy|sky|clouds|cloud|night|day|daytime|nighttime|sunset|sunrise|dusk|dawn|twilight|evening|morning|noon|afternoon|rain|rainy|raining|snow|snowing|snowy|fog|foggy|mist|misty|storm|thunderstorm|winter|summer|autumn|landscape|scenery|nature|bed|couch|sofa|chair|table|desk|window|wall|floor|ground|tree|trees|flowers|flower|petals|leaves|stars|starry|moon|sun|fire|flames|campfire|lava|ice|ship|boat|train|car|spaceship|tent|camp|farm|barn|stable|ranch|bath|bathtub|dojo|laboratory|lab|hospital|graveyard|cemetery|scene|environment|location|world|universe|interior|exterior|backdrop|horizon|skyline|rooftop|balcony|porch|courtyard|plaza|harbor|dock|pier|island|volcano|canyon|valley|tundra|savanna|glacier|oasis|farmland|countryside|suburb|downtown|subway|station|airport|hallway|corridor|staircase|stairs|basement|attic|garage|warehouse|factory|workshop|forge|mine|tower|bedsheet|pillow|carpet|rug|curtains|bookshelf|fireplace|lamp|candle|candles|mirror');

const CAMERA_EXACT = set('close-up|closeup|close up|extreme close-up|portrait|profile|upper body|lower body|full body|cowboy shot|head shot|headshot|wide shot|very wide shot|medium shot|long shot|establishing shot|from above|from below|from behind|from side|from the side|from front|from outside|from inside|dutch angle|low angle|high angle|bird\'s-eye view|birds eye view|bird\'s eye view|worm\'s-eye view|aerial view|overhead view|side view|front view|back view|rear view|three-quarter view|dynamic angle|fisheye|panorama|panoramic|foreshortening|pov|first-person view|first person view|depth of field|dof|bokeh|blurry|blurry background|blurry foreground|motion blur|chromatic aberration|film grain|vignette|vignetting|wide-angle|wide angle|telephoto|centered|symmetry|rule of thirds|cropped|out of frame|feet out of frame|head out of frame|multiple views|split screen|selfie|mirror selfie|zoom|zoomed in|zoomed out|looking at camera|pov hands|pov crotch|straight-on|tilted frame');
const CAMERA_WORDS = set('shot|angle|view|perspective|framing|composition|lens|camera|close-up|closeup|portrait|bokeh|blur|blurry|focus|fisheye|panorama|foreshortening|pov|zoom|cropped|telephoto|macro|photo-shoot');

const LIGHTING_WORDS = set('lighting|light|lights|lit|backlighting|backlit|sunlight|moonlight|candlelight|firelight|lamplight|starlight|sunbeam|sunbeams|rays|shadow|shadows|shade|glow|glowing|bloom|flare|chiaroscuro|silhouette|dark|darkness|dim|bright|contrast|neon|illumination|illuminated|reflection|reflections|sparkle|sparkles|sparkling|luminous|radiant|twinkling|gloomy|shaded|spotlight|rimlight|volumetric|caustics');
const LIGHTING_EXACT = set('rim light|rim lighting|volumetric lighting|cinematic lighting|dramatic lighting|soft lighting|studio lighting|natural light|natural lighting|god rays|light rays|crepuscular rays|hard shadows|lens flare|high contrast|low key|high key|neon lights|golden hour|blue hour|ambient occlusion|subsurface scattering|ray tracing|global illumination|dappled sunlight|light particles|soft light|hard light|warm light|cold light|colored light|backlight');

const STYLE_EXACT = set('anime|anime style|manga|cartoon|comic|comic style|realistic|realism|photorealistic|photorealism|hyperrealistic|semi-realistic|photo|photograph|photography|raw photo|3d|2d|cgi|render|octane render|unreal engine|blender|painting|oil painting|watercolor|watercolour|acrylic|gouache|pastel|digital painting|digital art|illustration|concept art|sketch|lineart|line art|ink|inking|monochrome|greyscale|grayscale|sepia|cel shading|cel-shaded|flat color|flat colors|pixel art|low poly|vector|traditional media|traditional art|impressionism|expressionism|art nouveau|art deco|ukiyo-e|baroque|renaissance|surreal|surrealism|minimalism|minimalist|abstract|pop art|graffiti|fantasy art|dark fantasy|fantasy|sci-fi|science fiction|cyberpunk|steampunk|gothic|retro|vintage|vaporwave|synthwave|chibi|kawaii|toon|disney|pixar|ghibli|studio ghibli|official art|game cg|screencap|anime screencap|anime coloring|colorful|vibrant colors|vibrant|muted colors|pastel colors|warm colors|cool colors|limited palette|high saturation|painterly|poster|wallpaper|album cover|magazine cover|cinematic|film still|movie still|studio photo|editorial|fashion photography|polaroid|analog|film|35mm film|kodak|fujifilm');
const STYLE_WORDS = set('style|styled|anime|manga|cartoon|comic|realistic|photorealistic|painting|watercolor|sketch|lineart|illustration|render|monochrome|greyscale|grayscale|sepia|art|artwork|drawing|drawn|painted|aesthetic|palette|shading|colors|colours|coloring|colouring|cel');

const META_EXACT = set('signature|watermark|text|english text|japanese text|artist name|web address|speech bubble|username|patreon username|twitter username|logo|border|letterboxed|jpeg artifacts|commentary|translated|dated|copyright name|character name|sound effects|onomatopoeia|caption|subtitles|censored|uncensored|mosaic censoring|bar censor|commission|patreon reward|paid reward|variant set|comic panel|4koma|lowres|scan|third-party edit|photoshop \\(medium\\)|absurdres');

const SUBJECT_COUNT_RE = /^(\d+\+?|multiple|many|several|two|three|four|five)\s?(girls?|boys?|others?|futas?|males?|females?|women|men|people|persons|characters)$/;
const DATE_RE = /^(year\s?)?(19|20)\d\d(s)?$/;
const SCORE_RE = /^score[\s_]?\d(\s?up)?$/;
const SOURCE_RE = /^source[\s_](anime|cartoon|furry|pony|comic|manga|real|photo|3d|western|game)/;
const RATING_RE = /^rating[\s_:]/;
const ARTIST_RE = /^(by|art by|artist)\s+(?!(the|a|an|his|her|their|my|your|its)\b)\S|^@\S|^artist:\s*\S/;
const LIMB_POSE_RE = /^(hand|hands|arm|arms|finger|fingers|paw|paws|leg|legs|foot|feet|knee|knees|head|tail) (on|in|behind|over|under|between|around|up|down|to|raised|lifted|out|together|apart)\b/;
const LENS_RE = /^\d+\s?mm(\s(lens|photo|photograph))?$|^f\/\d/;

/**
 * Ключ фрагмента для классификатора: без веса, скобок расписаний,
 * экранирования и подчёркиваний, в нижнем регистре.
 */
function classifierKey(seg) {
  if (!seg) return '';
  let base = seg.kind === 'weighted' ? seg.base : seg.text;
  if (seg.kind === 'schedule' || seg.kind === 'group') {
    base = String(seg.text || '')
      .replace(/^[[{(]+|[\]})]+$/g, '')
      .split(/[:|]/)
      .filter((part) => part.trim() && !/^\s*[-+]?\d*\.?\d+\s*$/.test(part))
      .join(' ');
  }
  return String(base || '')
    .replace(/\\([()[\]{}])/g, '$1')
    .replace(/_/g, ' ')
    .replace(/\s+/g, ' ')
    .trim()
    .toLowerCase();
}

const tokenize = (key) => key
  .replace(/[()[\]{}"«»“”]/g, ' ')
  .split(/[\s,;]+/)
  .map((w) => w.replace(/^[.'!?]+|[.'!?:]+$/g, ''))
  .filter(Boolean);

// Словари «по слову» в порядке проверки: первый найденный ответ побеждает
const WORD_TABLES = [
  ['species', SPECIES_WORDS],
  ['body', BODY_HEADS],
  ['clothing', CLOTHING_WORDS],
  ['expression', EXPRESSION_WORDS],
  ['interaction', INTERACTION_WORDS],
  ['pose', POSE_WORDS],
  ['setting', SETTING_WORDS],
  ['camera', CAMERA_WORDS],
  ['lighting', LIGHTING_WORDS],
  ['style', STYLE_WORDS],
  ['subject', SUBJECT_WORDS],
  ['body', BODY_WORDS],
];

function wordCategory(word) {
  for (const [category, table] of WORD_TABLES) {
    if (table.has(word)) return category;
  }
  if (/(style)$/.test(word) && word !== 'hairstyle') return 'style';
  return null;
}

function qualitySub(key) {
  if (SCORE_RE.test(key)) return 'score';
  if (SOURCE_RE.test(key)) return 'source';
  if (RATING_RE.test(key) || RATING_EXACT.has(key)) return 'rating';
  if (AESTHETIC_EXACT.has(key)) return 'aesthetic';
  if (DATE_EXACT.has(key) || DATE_RE.test(key)) return 'date';
  if (RESOLUTION_EXACT.has(key)) return 'resolution';
  if (QUALITY_EXACT.has(key)) return 'quality';
  if (/^embedding:/.test(key)) return 'embedding';
  return null;
}

/** Голосование по словам длинной фразы: какая группа встречается чаще. */
function voteCategory(key, rankOf) {
  const words = tokenize(key);
  const counts = new Map();
  const add = (cat, w) => counts.set(cat, (counts.get(cat) || 0) + w);
  // Фразы из 2–3 слов весят больше отдельных слов
  for (let n = 3; n >= 2; n--) {
    for (let i = 0; i + n <= words.length; i++) {
      const phrase = words.slice(i, i + n).join(' ');
      const cat = exactCategory(phrase);
      if (cat) add(cat, 2);
    }
  }
  words.forEach((w) => {
    if (/^(a|an|the|and|with|of|in|on|at|by|for|to|from|his|her|their|its|is|are|wearing)$/.test(w)) {
      if (w === 'wearing') add('clothing', 1);
      return;
    }
    const cat = wordCategory(w) || (SUBJECT_COUNT_RE.test(w) ? 'subject' : null);
    // Предложение, где назван субъект («a tall orc warrior…»), — про субъект
    if (cat) add(cat, cat === 'subject' || cat === 'species' ? 2 : 1);
  });
  let best = null;
  let bestScore = 0;
  counts.forEach((score, cat) => {
    if (score > bestScore || (score === bestScore && best && rankOf(cat) < rankOf(best))) {
      best = cat;
      bestScore = score;
    }
  });
  return best;
}

function exactCategory(key) {
  if (qualitySub(key)) return 'quality';
  if (SUBJECT_EXACT.has(key) || SUBJECT_COUNT_RE.test(key)) return 'subject';
  if (EXPRESSION_EXACT.has(key)) return 'expression';
  if (POSE_EXACT.has(key)) return 'pose';
  if (INTERACTION_EXACT.has(key)) return 'interaction';
  if (CAMERA_EXACT.has(key) || LENS_RE.test(key)) return 'camera';
  if (LIGHTING_EXACT.has(key)) return 'lighting';
  if (STYLE_EXACT.has(key)) return 'style';
  if (META_EXACT.has(key)) return 'meta';
  return null;
}

/**
 * Категория одного фрагмента.
 * ctx: { lookup(key) → {category} | null, triggerWords: Set<string>, family }
 * Возвращает { category, sub, source: 'syntax' | 'rule' | 'dict' | 'vote' | 'none' }.
 */
export function classifyPromptSegment(seg, ctx = {}) {
  if (!seg) return { category: 'unknown', sub: null, source: 'none' };
  if (seg.kind === 'keyword') return { category: 'keyword', sub: null, source: 'syntax' };
  if (seg.kind === 'lora' || seg.kind === 'extra') return { category: 'lora', sub: null, source: 'syntax' };
  const key = classifierKey(seg);
  if (!key) return { category: 'unknown', sub: null, source: 'none' };
  if (/^embedding:/.test(key)) return { category: 'quality', sub: 'embedding', source: 'rule' };

  const sub = qualitySub(key);
  if (sub) return { category: 'quality', sub, source: 'rule' };

  // Триггеры выбранных LoRA — отдельная группа сразу после персонажей
  const triggers = ctx.triggerWords;
  if (triggers && typeof triggers.has === 'function' && triggers.has(key)) {
    return { category: 'trigger', sub: null, source: 'rule' };
  }

  if (ARTIST_RE.test(key) && !/\bstyle\b/.test(key)) return { category: 'artist', sub: null, source: 'rule' };

  const exact = exactCategory(key);
  if (exact) return { category: exact, sub: null, source: 'rule' };

  // Словарь тегов: персонажи, серии и художники узнаются только по нему
  let entry = null;
  if (typeof ctx.lookup === 'function') {
    try { entry = ctx.lookup(key); } catch { entry = null; }
  }
  const dictCat = entry?.category;
  if (dictCat === 'character' || dictCat === 'copyright' || dictCat === 'artist') {
    return { category: dictCat, sub: null, source: 'dict' };
  }

  const words = tokenize(key);
  const rankOf = rankFunction(ctx.family || 'illustrious');
  // Длинные фразы (естественный язык) — голосованием по словам
  if (words.length >= 4) {
    const voted = voteCategory(key, rankOf);
    return voted
      ? { category: voted, sub: null, source: 'vote' }
      : { category: 'unknown', sub: null, source: 'none' };
  }

  // Префиксы: «looking …», «holding …», «from …», «hugging …»
  const first = words[0] || '';
  const last = words[words.length - 1] || '';
  if (/^(looking|facing|glancing|staring|gazing)$/.test(first)) return { category: 'expression', sub: null, source: 'rule' };
  if (/^(hugging|kissing|embracing|cuddling|groping|licking|biting|sucking|grabbing|touching|petting|spanking|tickling|penetrating|straddling|riding|carrying)$/.test(first)
    && words.length > 1 && (/\b(another|other|partner|each)\b/.test(key) || first !== 'riding')) {
    return { category: 'interaction', sub: null, source: 'rule' };
  }
  if (first === 'holding' || first === 'wielding') return { category: 'pose', sub: null, source: 'rule' };
  if (LIMB_POSE_RE.test(key)) return { category: 'pose', sub: null, source: 'rule' };
  if (first === 'from' && words.length > 1) return { category: 'camera', sub: null, source: 'rule' };
  if (/\b(another|other's|each other)\b/.test(key)) return { category: 'interaction', sub: null, source: 'rule' };
  if (/\bbackground$/.test(key)) return { category: 'setting', sub: null, source: 'rule' };
  if (/\bstyle$/.test(key) || /^style\b/.test(key)) return { category: 'style', sub: null, source: 'rule' };

  // «wolf boy», «anthro male» — вид; «muscular male» — телосложение
  if (/^(boy|girl|man|woman|guy|male|female|boys|girls|men|women|males|females)$/.test(last) && words.length > 1) {
    if (words.slice(0, -1).some((w) => SPECIES_WORDS.has(w))) return { category: 'species', sub: null, source: 'rule' };
    if (/^(male|female|males|females)$/.test(last)) return { category: 'body', sub: null, source: 'rule' };
  }

  // «scar on face», «tattoo on arm»: главное слово — до предлога
  const prep = /^(.+?) (on|over|across|around|under|in|with) (.+)$/.exec(key);
  if (prep) {
    const leftWords = tokenize(prep[1]);
    const leftCat = wordCategory(leftWords[leftWords.length - 1] || '');
    if (leftCat) return { category: leftCat, sub: null, source: 'rule' };
  }

  // Главное слово — последнее: «red dress» — одежда, «hair ornament» — тоже
  const headCat = wordCategory(last);
  if (headCat) return { category: headCat, sub: null, source: 'rule' };

  // Остальные слова справа налево: «spread wings» → поза (spread), и т.д.
  for (let i = words.length - 2; i >= 0; i--) {
    const cat = wordCategory(words[i]);
    if (cat) return { category: cat, sub: null, source: 'rule' };
  }

  // Глагол на -ing первым словом — действие («sitting on chair»)
  if (/^[a-z]{3,}ing$/.test(first) && !/^(lighting|clothing|evening|morning|ceiling|building|painting|drawing)$/.test(first)) {
    return { category: 'pose', sub: null, source: 'rule' };
  }

  if (dictCat === 'meta') return { category: 'meta', sub: null, source: 'dict' };
  // «name (series)» без словаря — обычно персонаж
  if (/\S \([^)]+\)$/.test(key)) return { category: 'character', sub: null, source: 'rule' };
  return { category: 'unknown', sub: null, source: 'none' };
}

/** Функция «место группы» для семейства (меньше — раньше). */
function rankFunction(family) {
  if (family === 'natural') {
    return (cat) => (cat in NATURAL_RANK ? NATURAL_RANK[cat] : 50);
  }
  const order = ORDERS[family] || ORDERS.illustrious;
  return (cat) => {
    const i = order.indexOf(cat);
    return i >= 0 ? i : 50;
  };
}

function subRankFunction(family) {
  const list = family === 'pony' ? QUALITY_SUB.pony : QUALITY_SUB.default;
  return (sub) => {
    const i = list.indexOf(sub || 'quality');
    return i >= 0 ? i : list.length;
  };
}

/** Категории всех фрагментов (с метками ИИ поверх нераспознанных). */
export function classifyAll(segments, ctx = {}) {
  const labels = ctx.labels || null;
  return segments.map((seg, i) => {
    const own = classifyPromptSegment(seg, ctx);
    if (own.category === 'unknown' && labels && labels[i] && PROMPT_CATEGORIES.includes(labels[i]) && labels[i] !== 'unknown') {
      return { category: labels[i], sub: null, source: 'ai' };
    }
    return own;
  });
}

/**
 * Нераспознанный фрагмент «прилипает» к предыдущему распознанному (или к
 * следующему, если стоит в начале блока): мы не знаем, куда его нести, и
 * оставляем рядом с тем, что человек написал вокруг.
 */
function effectiveCategories(indices, cats) {
  const out = new Map();
  let prev = null;
  const pending = [];
  indices.forEach((i) => {
    const c = cats[i].category;
    if (c === 'unknown') {
      if (prev) out.set(i, { ...prev, glued: true });
      else pending.push(i);
      return;
    }
    prev = cats[i];
    out.set(i, cats[i]);
    while (pending.length) out.set(pending.shift(), { ...cats[i], glued: true });
  });
  pending.forEach((i) => out.set(i, { category: 'unknown', sub: null, glued: true }));
  return out;
}

/** Блоки промпта, разделённые BREAK / AND: [[индексы сегментов], …]. */
function splitBlocks(segments) {
  const blocks = [[]];
  segments.forEach((seg, i) => {
    if (seg.kind === 'keyword') blocks.push([]);
    else blocks[blocks.length - 1].push(i);
  });
  return blocks.filter((b) => b.length);
}

/** Собрать текст, поставив в каждую позицию-слот текст другого сегмента. */
function rebuildWithPermutation(text, segments, slotToSource) {
  let out = '';
  let cursor = 0;
  segments.forEach((seg, idx) => {
    out += text.slice(cursor, seg.start);
    out += segments[slotToSource[idx]].text;
    cursor = seg.end;
  });
  out += text.slice(cursor);
  return out;
}

/**
 * Порядок сегментов блока по группам (стабильный: внутри группы — как было).
 * catOf(i) — категория сегмента; нераспознанные прилипают к предыдущему
 * распознанному (ведущие — к первому распознанному) и едут вместе с ним.
 */
function sortBlock(block, catOf, rankOf, subRankOf) {
  const groups = [];
  let pending = [];
  block.forEach((i, pos) => {
    const c = catOf(i);
    if (!c || c.category === 'unknown') {
      if (groups.length) groups[groups.length - 1].members.push(i);
      else pending.push(i);
      return;
    }
    groups.push({ key: c, members: [...pending, i], pos });
    pending = [];
  });
  if (pending.length) groups.push({ key: { category: 'unknown', sub: null }, members: pending, pos: -1 });
  groups.sort((a, b) => {
    const ra = rankOf(a.key.category);
    const rb = rankOf(b.key.category);
    if (ra !== rb) return ra - rb;
    if (a.key.category === 'quality' && b.key.category === 'quality') {
      const sa = subRankOf(a.key.sub);
      const sb = subRankOf(b.key.sub);
      if (sa !== sb) return sa - sb;
    }
    return a.pos - b.pos;
  });
  return groups.flatMap((g) => g.members);
}

const TERMINATOR_RE = /^[ \t]*[.!?。]/;

/**
 * Единицы естественного языка: предложения (до . ! ? или перевода строки).
 * Возвращает null, если предложение одно (тогда переставлять нечего, либо
 * это список тегов — решает вызывающий).
 */
function sentenceUnits(text, segments, block) {
  const units = [];
  let current = [];
  block.forEach((i, pos) => {
    current.push(i);
    const seg = segments[i];
    const nextSeg = pos + 1 < block.length ? segments[block[pos + 1]] : null;
    const gap = text.slice(seg.end, nextSeg ? nextSeg.start : text.length);
    if (!nextSeg || TERMINATOR_RE.test(gap) || gap.includes('\n')) {
      units.push(current);
      current = [];
    }
  });
  if (current.length) units.push(current);
  return units;
}

function organizeNaturalSentences(text, segments, block, units, cats, ctx, rankOf) {
  // Категория предложения — голосованием по его словам (или по меткам ИИ)
  const unitCats = units.map((unit) => {
    const first = segments[unit[0]];
    const last = segments[unit[unit.length - 1]];
    const body = text.slice(first.start, last.end);
    const labelled = unit.map((i) => cats[i]).filter((c) => c.source === 'ai');
    if (labelled.length) {
      return { category: labelled[0].category, sub: null };
    }
    const own = unit.length === 1 ? cats[unit[0]] : { category: voteCategory(body.toLowerCase(), rankOf) || 'unknown' };
    return { category: own.category || 'unknown', sub: own.sub || null };
  });
  const order = sortBlock(units.map((_, k) => k), (k) => unitCats[k], rankOf, () => 0);
  if (order.every((k, pos) => k === pos)) return null;

  // Текст единицы — от начала первого сегмента до конца (с точкой, если есть)
  const pieces = units.map((unit) => {
    const first = segments[unit[0]];
    const last = segments[unit[unit.length - 1]];
    let end = last.end;
    const m = TERMINATOR_RE.exec(text.slice(end));
    if (m) end += m[0].length;
    return { start: first.start, end, text: text.slice(first.start, end), terminated: !!m };
  });
  let out = text.slice(0, pieces[0].start);
  order.forEach((k, pos) => {
    const piece = pieces[k];
    const slot = pieces[pos];
    const gapEnd = pos + 1 < pieces.length ? pieces[pos + 1].start : text.length;
    const gap = text.slice(slot.end, gapEnd);
    let body = piece.text;
    // Предложение без точки уехало в середину — точка, чтобы не слиплось
    if (!piece.terminated && pos + 1 < pieces.length && !gap.includes('\n')) body += '.';
    out += body + gap;
  });
  return { text: out, unitOrder: order, units };
}

/**
 * Разложить промпт по местам.
 * options: { family, lookup, triggerWords, labels, dedupe }
 * Возвращает {
 *   text, changed, moved, segments, categories, unknown: [индексы],
 *   order: [индексы исходных сегментов в новом порядке], removed: [тексты]
 * }.
 */
export function organizePrompt(text, options = {}) {
  const src = typeof text === 'string' ? text : '';
  const family = ORDER_FAMILIES.includes(options.family) ? options.family : 'illustrious';
  const ctx = { ...options, family };
  const segments = segmentPrompt(src);
  const cats = classifyAll(segments, ctx);
  const rankOf = rankFunction(family);
  const subRankOf = subRankFunction(family);
  const unknown = cats.map((c, i) => (c.category === 'unknown' ? i : -1)).filter((i) => i >= 0);
  const slotToSource = segments.map((_, i) => i);
  let out = src;
  let mode = 'tags';

  const blocks = splitBlocks(segments);
  let naturalDone = false;
  if (family === 'natural' && blocks.length) {
    // Естественный язык: если это не список коротких тегов, переставляем
    // только целые предложения, и только в промпте без BREAK/AND.
    const tagLike = segments.every((s) => s.kind !== 'tag' || s.words <= 3);
    if (!tagLike) {
      mode = 'sentences';
      naturalDone = true;
      const single = blocks.length === 1 && blocks[0].length === segments.length;
      const units = single ? sentenceUnits(src, segments, blocks[0]) : null;
      if (units && units.length > 1) {
        const r = organizeNaturalSentences(src, segments, blocks[0], units, cats, ctx, rankOf);
        if (r) {
          out = r.text;
          r.unitOrder.flatMap((k) => units[k]).forEach((sourceIndex, pos) => { slotToSource[pos] = sourceIndex; });
        }
      }
    }
  }

  if (!naturalDone) {
    blocks.forEach((block) => {
      const order = sortBlock(block, (i) => cats[i], rankOf, subRankOf);
      block.forEach((slot, pos) => { slotToSource[slot] = order[pos]; });
    });
    out = rebuildWithPermutation(src, segments, slotToSource);
  }

  let removed = [];
  if (options.dedupe) {
    const r = removeDuplicates(out);
    out = r.text;
    removed = r.removed;
  }
  const order = slotToSource.filter((i) => segments[i].kind !== 'keyword');
  const moved = slotToSource.reduce((n, source, slot) => n + (source !== slot ? 1 : 0), 0);
  return {
    text: out,
    changed: out !== src,
    moved,
    mode,
    family,
    segments,
    categories: cats,
    unknown,
    order,
    // slots[k] — индекс исходного сегмента, стоящего теперь на месте k
    slots: slotToSource,
    removed,
  };
}

/** Убрать повторы (тот же тег без учёта веса/регистра) — остаётся первый. */
export function removeDuplicates(text) {
  let out = typeof text === 'string' ? text : '';
  const removed = [];
  const segs = segmentPrompt(out);
  const seen = new Set();
  const drop = [];
  segs.forEach((seg, i) => {
    if (seg.kind === 'keyword') return;
    const key = normalizeKey(seg);
    if (!key) return;
    if (seen.has(key)) drop.push(i);
    else seen.add(key);
  });
  for (let k = drop.length - 1; k >= 0; k--) {
    const segsNow = segmentPrompt(out);
    const i = drop[k];
    removed.unshift(segsNow[i].text);
    const seg = segsNow[i];
    const prev = segsNow[i - 1];
    const next = segsNow[i + 1];
    if (prev) out = out.slice(0, prev.end) + out.slice(seg.end);
    else if (next) out = out.slice(0, seg.start) + out.slice(next.start);
    else out = out.slice(0, seg.start) + out.slice(seg.end);
  }
  return { text: out, removed };
}

/**
 * Проверка «ничего не потеряно»: в результате ровно те же фрагменты
 * (мультимножество точных строк), только в другом порядке.
 */
export function isPermutationOfSegments(before, after) {
  const a = segmentPrompt(String(before || '')).map((s) => s.text);
  const b = segmentPrompt(String(after || '')).map((s) => s.text);
  if (a.length !== b.length) return false;
  const counts = new Map();
  a.forEach((x) => counts.set(x, (counts.get(x) || 0) + 1));
  for (const x of b) {
    const n = counts.get(x);
    if (!n) return false;
    counts.set(x, n - 1);
  }
  return true;
}

/* ─────────────────────── Вставка и точечные сдвиги ─────────────────────── */

function blockOf(segments, index) {
  let start = 0;
  for (let i = 0; i < segments.length; i++) {
    if (segments[i].kind === 'keyword') {
      if (i >= index) break;
      start = i + 1;
    }
  }
  let end = segments.length - 1;
  for (let i = Math.max(index, start); i < segments.length; i++) {
    if (segments[i].kind === 'keyword') { end = i - 1; break; }
  }
  const out = [];
  for (let i = start; i <= end; i++) out.push(i);
  return out;
}

/**
 * Куда встать фрагменту категории cat в блоке (без самого фрагмента skip):
 * после последнего фрагмента своей или более ранней группы, иначе — перед
 * первым фрагментом блока. Возвращает { after } или { before }.
 */
function logicalSlot(block, cats, cat, rankOf, subRankOf, skip = -1) {
  const others = block.filter((i) => i !== skip);
  if (!others.length) return null;
  const eff = effectiveCategories(others, cats);
  // Составной ранг: группа, а внутри качества — подгруппа (score → source …)
  const keyOf = (c) => rankOf(c.category) * 100 + (c.category === 'quality' ? subRankOf(c.sub) : 0);
  const mine = keyOf(cat);
  const keys = others.map((i) => keyOf(eff.get(i)));
  // Позиция с наименьшим числом «нарушений порядка» относительно фрагмента:
  // слева не должно быть групп позже, справа — раньше. В упорядоченном
  // промпте это ровно «после своей группы»; в хаотичном — ближайшее к
  // правильному место, а не просто «после последнего подходящего».
  let later = 0; // слева от позиции: ключ больше моего
  let earlier = keys.filter((k) => k < mine).length; // справа: ключ меньше
  let best = { cost: later + earlier, pos: 0 };
  keys.forEach((k, idx) => {
    if (k > mine) later += 1;
    if (k < mine) earlier -= 1;
    const cost = later + earlier;
    // При равенстве — правее: новый фрагмент встаёт после своей группы
    if (cost <= best.cost) best = { cost, pos: idx + 1 };
  });
  return best.pos > 0 ? { after: others[best.pos - 1] } : { before: others[0] };
}

/**
 * Вставить тег «на своё место» (подсказки Каданса, триггеры LoRA).
 * Уже есть в промпте — возвращает { text, existed: true }. Нераспознанный
 * тег и промпт-предложения естественного языка — в конец, как раньше.
 * options: { family, lookup, triggerWords, category }
 */
export function insertTagLogically(text, piece, options = {}) {
  const src = typeof text === 'string' ? text : '';
  const clean = cleanInsert(piece);
  if (!clean) return null;
  const segments = segmentPrompt(src);
  if (!segments.length) return { text: clean, start: 0, end: clean.length };
  const pieceSeg = segmentPrompt(clean);
  const pieceKey = pieceSeg.length === 1 ? normalizeKey(pieceSeg[0]) : normalizeKey(clean);
  const existing = segments.findIndex((s) => pieceKey && normalizeKey(s) === pieceKey);
  if (existing >= 0) {
    return { text: src, start: segments[existing].start, end: segments[existing].end, existed: true };
  }
  const family = ORDER_FAMILIES.includes(options.family) ? options.family : 'illustrious';
  const ctx = { ...options, family };
  const cat = options.category
    ? { category: options.category, sub: null }
    : (pieceSeg.length === 1 ? classifyPromptSegment(pieceSeg[0], ctx) : { category: 'unknown', sub: null });
  const naturalSentences = family === 'natural' && segments.some((s) => s.sentence);
  if (cat.category === 'unknown' || naturalSentences) return { ...appendSegment(src, clean), category: cat.category };

  const cats = classifyAll(segments, ctx);
  const block = blockOf(segments, 0);
  const slot = logicalSlot(block, cats, cat, rankFunction(family), subRankFunction(family));
  if (!slot) return { ...appendSegment(src, clean), category: cat.category };
  if (slot.after !== undefined) {
    return { ...insertAfterSegment(src, segments, slot.after, clean), category: cat.category };
  }
  const s = segments[slot.before];
  const out = `${src.slice(0, s.start)}${clean}, ${src.slice(s.start)}`;
  return { text: out, start: s.start, end: s.start + clean.length, category: cat.category };
}

/**
 * Индекс, куда moveSegment должен перенести фрагмент index, чтобы он встал
 * «на логичное место». null — фрагмент не распознан или уже на месте.
 */
export function logicalMoveTarget(text, segments, index, options = {}) {
  const segs = segments || segmentPrompt(text);
  const seg = segs[index];
  if (!seg || seg.kind === 'keyword') return null;
  const family = ORDER_FAMILIES.includes(options.family) ? options.family : 'illustrious';
  const ctx = { ...options, family };
  const cats = classifyAll(segs, ctx);
  const cat = cats[index];
  if (cat.category === 'unknown') return null;
  const block = blockOf(segs, index);
  const slot = logicalSlot(block, cats, cat, rankFunction(family), subRankFunction(family), index);
  if (!slot) return null;
  let to;
  if (slot.after !== undefined) to = slot.after < index ? slot.after + 1 : slot.after;
  else to = slot.before < index ? slot.before : slot.before - 1;
  return to === index ? null : to;
}

/** Перенести фрагмент на логичное место: результат moveSegment или null. */
export function moveSegmentLogically(text, segments, index, options = {}) {
  const segs = segments || segmentPrompt(text);
  const to = logicalMoveTarget(text, segs, index, options);
  if (to === null) return null;
  return moveSegment(text, segs, index, to);
}

/**
 * «На группу раньше/позже»: перепрыгнуть соседей своей группы и одну
 * соседнюю группу целиком. dir = -1 | 1. Возвращает индекс для moveSegment.
 */
export function categoryStepTarget(text, segments, index, dir, options = {}) {
  const segs = segments || segmentPrompt(text);
  if (!segs[index] || segs[index].kind === 'keyword') return null;
  const family = ORDER_FAMILIES.includes(options.family) ? options.family : 'illustrious';
  const cats = classifyAll(segs, { ...options, family });
  const block = blockOf(segs, index);
  const eff = effectiveCategories(block, cats);
  const catAt = (i) => eff.get(i)?.category;
  const own = catAt(index);
  const lo = block[0];
  const hi = block[block.length - 1];
  let j = index + dir;
  while (j >= lo && j <= hi && catAt(j) === own) j += dir;
  if (j < lo || j > hi) {
    const edge = dir < 0 ? lo : hi;
    return edge === index ? null : edge;
  }
  const other = catAt(j);
  while (j >= lo && j <= hi && catAt(j) === other) j += dir;
  const to = j - dir;
  return to === index ? null : to;
}

/** Подпись порядка для семейства (ключи i18n: promptEditor.organize.rule.*). */
export function orderPreviewCategories(family) {
  if (family === 'natural') {
    return ['subject', 'body', 'pose', 'setting', 'camera', 'lighting', 'style', 'quality'];
  }
  return (ORDERS[family] || ORDERS.illustrious).filter((c) => c !== 'lora');
}
