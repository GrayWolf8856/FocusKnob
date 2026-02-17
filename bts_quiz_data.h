#ifndef BTS_QUIZ_DATA_H
#define BTS_QUIZ_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    QUIZ_EASY = 0,
    QUIZ_MEDIUM = 1,
    QUIZ_HARD = 2
} quiz_difficulty_t;

typedef struct {
    const char* question;
    const char* answers[4];
    uint8_t correct;           // 0-3
    quiz_difficulty_t difficulty;
} bts_question_t;

#define BTS_QUESTION_COUNT 200

static const bts_question_t BTS_QUESTIONS[BTS_QUESTION_COUNT] = {

    // ═══════════════════════════════════════════════════════════
    // EASY (questions 0-69) — Basic BTS facts
    // ═══════════════════════════════════════════════════════════

    // 0
    {"How many members are in BTS?",
     {"5", "6", "7", "8"}, 2, QUIZ_EASY},
    // 1
    {"What year did BTS debut?",
     {"2012", "2013", "2014", "2015"}, 1, QUIZ_EASY},
    // 2
    {"What does BTS stand for in Korean?",
     {"Beyond The Scene", "Bangtan Sonyeondan", "Boys That Sing", "Born To Shine"}, 1, QUIZ_EASY},
    // 3
    {"Which company manages BTS?",
     {"SM Entertainment", "YG Entertainment", "BigHit / HYBE", "JYP Entertainment"}, 2, QUIZ_EASY},
    // 4
    {"Who is the leader of BTS?",
     {"Jin", "Suga", "RM", "J-Hope"}, 2, QUIZ_EASY},
    // 5
    {"What is BTS's fandom called?",
     {"Blink", "ARMY", "Once", "Exo-L"}, 1, QUIZ_EASY},
    // 6
    {"Which member is the oldest?",
     {"RM", "Jin", "Suga", "J-Hope"}, 1, QUIZ_EASY},
    // 7
    {"Which member is the youngest (maknae)?",
     {"V", "Jimin", "Jungkook", "J-Hope"}, 2, QUIZ_EASY},
    // 8
    {"What is RM's real name?",
     {"Kim Namjoon", "Kim Seokjin", "Min Yoongi", "Jung Hoseok"}, 0, QUIZ_EASY},
    // 9
    {"What is Jin's real name?",
     {"Kim Namjoon", "Kim Seokjin", "Park Jimin", "Kim Taehyung"}, 1, QUIZ_EASY},
    // 10
    {"What is Suga's real name?",
     {"Jung Hoseok", "Jeon Jungkook", "Min Yoongi", "Kim Taehyung"}, 2, QUIZ_EASY},
    // 11
    {"What is J-Hope's real name?",
     {"Jung Hoseok", "Min Yoongi", "Park Jimin", "Kim Namjoon"}, 0, QUIZ_EASY},
    // 12
    {"What is Jimin's real name?",
     {"Kim Taehyung", "Jeon Jungkook", "Park Jimin", "Min Yoongi"}, 2, QUIZ_EASY},
    // 13
    {"What is V's real name?",
     {"Park Jimin", "Kim Taehyung", "Jeon Jungkook", "Kim Seokjin"}, 1, QUIZ_EASY},
    // 14
    {"What is Jungkook's real name?",
     {"Kim Taehyung", "Park Jimin", "Min Yoongi", "Jeon Jungkook"}, 3, QUIZ_EASY},
    // 15
    {"Which BTS song was their first #1 on the Billboard Hot 100?",
     {"Boy With Luv", "Dynamite", "Butter", "ON"}, 1, QUIZ_EASY},
    // 16
    {"What language is 'Dynamite' sung in?",
     {"Korean", "English", "Japanese", "Both Korean and English"}, 1, QUIZ_EASY},
    // 17
    {"Which BTS member is known as 'Worldwide Handsome'?",
     {"V", "Jin", "Jungkook", "RM"}, 1, QUIZ_EASY},
    // 18
    {"What is Jungkook's nickname?",
     {"Worldwide Handsome", "Golden Maknae", "Sunshine", "Genius"}, 1, QUIZ_EASY},
    // 19
    {"Which member is known as the 'Sunshine' of BTS?",
     {"Jimin", "V", "J-Hope", "Jin"}, 2, QUIZ_EASY},
    // 20
    {"What country is BTS from?",
     {"Japan", "China", "South Korea", "Thailand"}, 2, QUIZ_EASY},
    // 21
    {"What is the BTS lightstick called?",
     {"Candy Bong", "Army Bomb", "Light Ring", "Star Wand"}, 1, QUIZ_EASY},
    // 22
    {"Which member has a solo song called 'Lie'?",
     {"V", "Jimin", "Jungkook", "Suga"}, 1, QUIZ_EASY},
    // 23
    {"Which member raps in BTS?",
     {"Jin, Jimin, V", "RM, Suga, J-Hope", "Jungkook, V, RM", "All seven members"}, 1, QUIZ_EASY},
    // 24
    {"What is the name of BTS's reality show?",
     {"Run BTS", "BTS World", "Bangtan TV", "BTS House"}, 0, QUIZ_EASY},
    // 25
    {"What award show is known as the Korean Grammy?",
     {"MAMA", "Melon Music Awards", "Korean Music Awards", "Golden Disc Awards"}, 2, QUIZ_EASY},
    // 26
    {"What does ARMY stand for?",
     {"Always Ready My Youth", "Adorable Representative MC for Youth", "Amazing Radiant Musical Youth", "All Ready for Music and Youth"}, 1, QUIZ_EASY},
    // 27
    {"Which member has the stage name 'V'?",
     {"Kim Seokjin", "Kim Taehyung", "Park Jimin", "Jung Hoseok"}, 1, QUIZ_EASY},
    // 28
    {"What color represents BTS's official fandom?",
     {"Red", "Purple", "Blue", "Pink"}, 1, QUIZ_EASY},
    // 29
    {"What is BTS's greeting phrase?",
     {"We are one!", "Let's go!", "Bangtan! Annyeonghaseyo, BTS-imnida!", "Hello World!"}, 2, QUIZ_EASY},
    // 30
    {"Which song features the lyrics 'I'm the one I should love'?",
     {"IDOL", "Epiphany", "Fake Love", "DNA"}, 1, QUIZ_EASY},
    // 31
    {"What is BTS's debut single?",
     {"Boy In Luv", "No More Dream", "We Are Bulletproof", "N.O"}, 1, QUIZ_EASY},
    // 32
    {"Which member attended Konkuk University?",
     {"RM", "V", "Jungkook", "Jin"}, 0, QUIZ_EASY},
    // 33
    {"What genre is BTS primarily known for?",
     {"Rock", "K-pop / Hip-hop", "EDM", "R&B"}, 1, QUIZ_EASY},
    // 34
    {"Which platform do BTS frequently communicate with fans?",
     {"Twitter only", "Weverse", "Facebook", "TikTok only"}, 1, QUIZ_EASY},
    // 35
    {"What is BTS's English meaning (rebranded)?",
     {"Best Team Stars", "Beyond The Scene", "Boys To Success", "Brave and True Singers"}, 1, QUIZ_EASY},
    // 36
    {"Which BTS song has a DNA-themed music video?",
     {"Fake Love", "DNA", "Blood Sweat & Tears", "Spring Day"}, 1, QUIZ_EASY},
    // 37
    {"What is Suga's rapper alter-ego name?",
     {"Rap Monster", "Agust D", "Supreme Boi", "Gloss"}, 1, QUIZ_EASY},
    // 38
    {"Which member is from Busan?",
     {"RM and Suga", "Jimin and Jungkook", "V and Jin", "J-Hope and Suga"}, 1, QUIZ_EASY},
    // 39
    {"Who is the main dancer of BTS?",
     {"RM", "Jungkook", "Jimin", "Suga"}, 2, QUIZ_EASY},
    // 40
    {"Which song was BTS's comeback for Map of the Soul: 7?",
     {"Black Swan", "ON", "Filter", "My Time"}, 1, QUIZ_EASY},
    // 41
    {"What year did BTS first attend the Grammy Awards?",
     {"2018", "2019", "2020", "2021"}, 2, QUIZ_EASY},
    // 42
    {"Which BTS song mentions 'Fake Love'?",
     {"Fake Love", "DNA", "IDOL", "Boy With Luv"}, 0, QUIZ_EASY},
    // 43
    {"Which member is the lead dancer alongside Jimin?",
     {"Jin", "J-Hope", "Suga", "RM"}, 1, QUIZ_EASY},
    // 44
    {"What is the name of BTS's mobile game?",
     {"BTS World", "BTS Universe", "Bangtan Quest", "ARMY Run"}, 0, QUIZ_EASY},
    // 45
    {"Which member is known for his deep voice?",
     {"Jimin", "V", "J-Hope", "RM"}, 1, QUIZ_EASY},
    // 46
    {"What is Jin's animal nickname?",
     {"Bunny", "Bear", "Alpaca / RJ", "Cat"}, 2, QUIZ_EASY},
    // 47
    {"What is Jungkook's animal nickname?",
     {"Bunny", "Cat", "Dog", "Tiger"}, 0, QUIZ_EASY},
    // 48
    {"Which BTS album includes 'Spring Day'?",
     {"Wings", "You Never Walk Alone", "Love Yourself: Her", "Map of the Soul: 7"}, 1, QUIZ_EASY},
    // 49
    {"What year was 'Butter' released?",
     {"2019", "2020", "2021", "2022"}, 2, QUIZ_EASY},
    // 50
    {"Which member wrote the song 'Epiphany'?",
     {"RM", "Suga", "Slow Rabbit (for Jin)", "J-Hope"}, 2, QUIZ_EASY},
    // 51
    {"Which member is from Gwangju?",
     {"Jin", "J-Hope", "RM", "V"}, 1, QUIZ_EASY},
    // 52
    {"Where is BTS's company HYBE headquartered?",
     {"Busan", "Seoul", "Daegu", "Incheon"}, 1, QUIZ_EASY},
    // 53
    {"What is the BT21 character created by RM?",
     {"Tata", "Koya", "Chimmy", "Cooky"}, 1, QUIZ_EASY},
    // 54
    {"What is the BT21 character created by V?",
     {"Tata", "Koya", "RJ", "Mang"}, 0, QUIZ_EASY},
    // 55
    {"What is the BT21 character created by Jungkook?",
     {"Chimmy", "Cooky", "Shooky", "Mang"}, 1, QUIZ_EASY},
    // 56
    {"What is the BT21 character created by Jin?",
     {"RJ", "Tata", "Cooky", "Chimmy"}, 0, QUIZ_EASY},
    // 57
    {"Which BTS member appeared in the K-drama 'Hwarang'?",
     {"Jimin", "Jungkook", "V", "Jin"}, 2, QUIZ_EASY},
    // 58
    {"What is the BT21 character created by Suga?",
     {"Mang", "Shooky", "Koya", "RJ"}, 1, QUIZ_EASY},
    // 59
    {"What is the BT21 character created by J-Hope?",
     {"Shooky", "Mang", "Chimmy", "Tata"}, 1, QUIZ_EASY},
    // 60
    {"What is the BT21 character created by Jimin?",
     {"Chimmy", "Cooky", "Shooky", "Koya"}, 0, QUIZ_EASY},
    // 61
    {"Which member studied in New Zealand?",
     {"Jimin", "RM", "Jungkook", "V"}, 1, QUIZ_EASY},
    // 62
    {"Which song has the lyric 'I need you girl'?",
     {"I Need U", "Run", "Save ME", "Dope"}, 0, QUIZ_EASY},
    // 63
    {"What is RM's birth year?",
     {"1993", "1994", "1995", "1992"}, 1, QUIZ_EASY},
    // 64
    {"What is Jungkook's birth year?",
     {"1995", "1996", "1997", "1998"}, 2, QUIZ_EASY},
    // 65
    {"Which award did BTS win at the 2018 Billboard Music Awards?",
     {"Top Hot 100 Song", "Top Social Artist", "Top Duo/Group", "Top Selling Song"}, 1, QUIZ_EASY},
    // 66
    {"What reality show did BTS appear on before debut?",
     {"Rookie King", "BTS Bon Voyage", "Run BTS", "In the SOOP"}, 0, QUIZ_EASY},
    // 67
    {"Which member is left-handed?",
     {"RM", "V", "Jimin", "Suga"}, 1, QUIZ_EASY},
    // 68
    {"What is Jin's birth year?",
     {"1991", "1992", "1993", "1994"}, 1, QUIZ_EASY},
    // 69
    {"Which BTS song was used for a UNICEF campaign?",
     {"IDOL", "Love Myself", "Answer: Love Myself", "Euphoria"}, 2, QUIZ_EASY},

    // ═══════════════════════════════════════════════════════════
    // MEDIUM (questions 70-139) — Albums, songs, achievements
    // ═══════════════════════════════════════════════════════════

    // 70
    {"What was BTS's first studio album?",
     {"Dark & Wild", "Wings", "O!RUL8,2?", "Skool Luv Affair"}, 0, QUIZ_MEDIUM},
    // 71
    {"Which album contains the song 'Blood Sweat & Tears'?",
     {"The Most Beautiful Moment in Life Pt. 2", "Wings", "Love Yourself: Her", "You Never Walk Alone"}, 1, QUIZ_MEDIUM},
    // 72
    {"What was BTS's first song to reach #1 on Billboard Hot 100?",
     {"Boy With Luv", "ON", "Dynamite", "Life Goes On"}, 2, QUIZ_MEDIUM},
    // 73
    {"Which member's solo song is 'Euphoria'?",
     {"Jimin", "V", "Jungkook", "Jin"}, 2, QUIZ_MEDIUM},
    // 74
    {"Which BTS song was their UNICEF 'Love Myself' campaign theme?",
     {"Magic Shop", "Answer: Love Myself", "Mikrokosmos", "Love Maze"}, 1, QUIZ_MEDIUM},
    // 75
    {"What year did BTS first speak at the United Nations?",
     {"2017", "2018", "2019", "2020"}, 1, QUIZ_MEDIUM},
    // 76
    {"Which album is 'Boy With Luv' from?",
     {"Love Yourself: Answer", "Map of the Soul: Persona", "BE", "Map of the Soul: 7"}, 1, QUIZ_MEDIUM},
    // 77
    {"Who features on 'Boy With Luv'?",
     {"Halsey", "Sia", "Ed Sheeran", "Nicki Minaj"}, 0, QUIZ_MEDIUM},
    // 78
    {"Which member produced the song 'Intro: Persona'?",
     {"Suga", "RM", "J-Hope", "Pdogg"}, 1, QUIZ_MEDIUM},
    // 79
    {"What is the name of BTS's travel show?",
     {"Run BTS", "BTS Bon Voyage", "In the SOOP", "BTS Island"}, 1, QUIZ_MEDIUM},
    // 80
    {"Which member released the mixtape 'Hope World'?",
     {"RM", "Suga", "J-Hope", "Jungkook"}, 2, QUIZ_MEDIUM},
    // 81
    {"What was BTS's debut showcase called?",
     {"2 Cool 4 Skool", "O!RUL8,2?", "BTS Begins", "The Red Bullet"}, 0, QUIZ_MEDIUM},
    // 82
    {"Which award show did BTS attend in the US for the first time in 2017?",
     {"Grammy Awards", "Billboard Music Awards", "AMAs", "MTV VMAs"}, 1, QUIZ_MEDIUM},
    // 83
    {"What is the trilogy of albums called 'The Most Beautiful Moment in Life'?",
     {"Love Yourself", "Hwa Yang Yeon Hwa", "Map of the Soul", "Youth"}, 1, QUIZ_MEDIUM},
    // 84
    {"Which member sang 'Singularity'?",
     {"Jimin", "V", "Jungkook", "Jin"}, 1, QUIZ_MEDIUM},
    // 85
    {"What does 'Hwa Yang Yeon Hwa' mean?",
     {"Love Yourself", "The Most Beautiful Moment in Life", "Map of the Soul", "Bulletproof"}, 1, QUIZ_MEDIUM},
    // 86
    {"Which member wrote and produced 'Trivia: Love'?",
     {"Suga", "J-Hope", "RM", "Jimin"}, 2, QUIZ_MEDIUM},
    // 87
    {"What was BTS's first world tour called?",
     {"Wings Tour", "The Red Bullet", "Love Yourself Tour", "Map of the Soul Tour"}, 1, QUIZ_MEDIUM},
    // 88
    {"Which BTS song was the first Korean song to debut at #1 on Billboard Hot 100?",
     {"Dynamite", "Life Goes On", "Butter", "Permission to Dance"}, 1, QUIZ_MEDIUM},
    // 89
    {"What is the sub-unit of RM, Suga, and J-Hope called?",
     {"Vocal line", "Rap line", "Maknae line", "Hyung line"}, 1, QUIZ_MEDIUM},
    // 90
    {"Which album contains 'Mic Drop'?",
     {"Wings", "Love Yourself: Her", "You Never Walk Alone", "Dark & Wild"}, 1, QUIZ_MEDIUM},
    // 91
    {"Who remixed 'Mic Drop'?",
     {"Marshmello", "Steve Aoki", "DJ Snake", "Skrillex"}, 1, QUIZ_MEDIUM},
    // 92
    {"What concert film was released in theaters in 2018?",
     {"Break the Silence", "Burn the Stage", "Bring the Soul", "Yet to Come in Cinemas"}, 1, QUIZ_MEDIUM},
    // 93
    {"Which member's solo in Wings is 'MAMA'?",
     {"Jimin", "V", "J-Hope", "Jin"}, 2, QUIZ_MEDIUM},
    // 94
    {"What is the sub-unit of Jin, Jimin, V, and Jungkook called?",
     {"Rap line", "Vocal line", "Dance line", "Maknae line"}, 1, QUIZ_MEDIUM},
    // 95
    {"Which song starts with 'I'm exhausted man'?",
     {"Dope", "Silver Spoon", "Pied Piper", "Go Go"}, 1, QUIZ_MEDIUM},
    // 96
    {"What year did BTS release 'Love Yourself: Her'?",
     {"2016", "2017", "2018", "2019"}, 1, QUIZ_MEDIUM},
    // 97
    {"Which BTS song was choreographed blindfolded?",
     {"Black Swan", "ON", "Fake Love", "Blood Sweat & Tears"}, 0, QUIZ_MEDIUM},
    // 98
    {"What was BTS's first Japanese single?",
     {"No More Dream (Japanese)", "Boy In Luv (Japanese)", "For You", "Crystal Snow"}, 0, QUIZ_MEDIUM},
    // 99
    {"Which member is from Daegu?",
     {"Jin and RM", "Suga and V", "Jimin and J-Hope", "Jungkook and V"}, 1, QUIZ_MEDIUM},
    // 100
    {"What is the name of the docu-series about BTS's hiatus announcement?",
     {"Break the Silence", "Burn the Stage", "BTS Monuments: Beyond the Star", "Yet to Come"}, 2, QUIZ_MEDIUM},
    // 101
    {"Which album features 'Life Goes On'?",
     {"Map of the Soul: 7", "BE", "Butter", "Proof"}, 1, QUIZ_MEDIUM},
    // 102
    {"What year did BTS release their anthology album 'Proof'?",
     {"2021", "2022", "2023", "2020"}, 1, QUIZ_MEDIUM},
    // 103
    {"Which BTS member released the solo album 'Jack in the Box'?",
     {"RM", "Suga", "J-Hope", "Jimin"}, 2, QUIZ_MEDIUM},
    // 104
    {"What was Jimin's debut solo album called?",
     {"FACE", "Like Crazy", "Set Me Free", "Letter"}, 0, QUIZ_MEDIUM},
    // 105
    {"Which BTS song featured Nicki Minaj?",
     {"Mic Drop", "IDOL", "Boy With Luv", "Butter"}, 1, QUIZ_MEDIUM},
    // 106
    {"What is the name of BTS's camping/rest show?",
     {"Bon Voyage", "In the SOOP", "Run BTS", "BTS Village"}, 1, QUIZ_MEDIUM},
    // 107
    {"Which member has a pet dog named Yeontan?",
     {"Jimin", "V", "Jungkook", "Jin"}, 1, QUIZ_MEDIUM},
    // 108
    {"What is RM's solo debut album called?",
     {"Mono", "Indigo", "RM", "Namjoon"}, 1, QUIZ_MEDIUM},
    // 109
    {"Which BTS song was released as a 'summer song' in 2021?",
     {"Dynamite", "Permission to Dance", "Butter", "Life Goes On"}, 1, QUIZ_MEDIUM},
    // 110
    {"What year did BTS perform at Wembley Stadium?",
     {"2018", "2019", "2020", "2021"}, 1, QUIZ_MEDIUM},
    // 111
    {"Which member released the solo album 'D-2'?",
     {"RM", "Suga (Agust D)", "J-Hope", "Jungkook"}, 1, QUIZ_MEDIUM},
    // 112
    {"Which BTS song features sign language choreography?",
     {"Dynamite", "Butter", "Permission to Dance", "Life Goes On"}, 2, QUIZ_MEDIUM},
    // 113
    {"What was the first BTS song to get a Billboard Music Award nomination?",
     {"DNA", "Fire", "Spring Day", "Blood Sweat & Tears"}, 0, QUIZ_MEDIUM},
    // 114
    {"Which member composed 'Still With You'?",
     {"Jimin", "V", "Jungkook", "Suga"}, 2, QUIZ_MEDIUM},
    // 115
    {"What is the last track on 'Love Yourself: Answer'?",
     {"Answer: Love Myself", "Epiphany", "IDOL", "I'm Fine"}, 0, QUIZ_MEDIUM},
    // 116
    {"Which member released 'Christmas Tree' for a K-drama OST?",
     {"Jin", "V", "Jimin", "Jungkook"}, 1, QUIZ_MEDIUM},
    // 117
    {"What year did BTS hold their free concert 'Yet To Come' in Busan?",
     {"2021", "2022", "2023", "2020"}, 1, QUIZ_MEDIUM},
    // 118
    {"Which BTS song has over 1 billion views on YouTube first?",
     {"DNA", "Dynamite", "Boy With Luv", "MIC Drop"}, 1, QUIZ_MEDIUM},
    // 119
    {"What was the name of BTS's Las Vegas residency concerts?",
     {"Love Yourself in Vegas", "Permission to Dance on Stage", "Map of the Soul Tour", "BTS Live in Vegas"}, 1, QUIZ_MEDIUM},
    // 120
    {"Which member majored in film/acting at university?",
     {"Jin", "V", "RM", "Jimin"}, 1, QUIZ_MEDIUM},
    // 121
    {"What was the first BTS album to reach #1 on the Billboard 200?",
     {"Love Yourself: Tear", "Love Yourself: Answer", "Map of the Soul: Persona", "Wings"}, 0, QUIZ_MEDIUM},
    // 122
    {"Which BTS song features the lyric 'You can't stop me lovin myself'?",
     {"IDOL", "Dynamite", "Not Today", "Anpanman"}, 0, QUIZ_MEDIUM},
    // 123
    {"What was V's first solo album?",
     {"Layover", "Rainy Days", "Slow Dancing", "Blue"}, 0, QUIZ_MEDIUM},
    // 124
    {"Which member released 'GOLDEN' as a solo album?",
     {"Jimin", "V", "Jungkook", "Jin"}, 2, QUIZ_MEDIUM},
    // 125
    {"What is the name of Jin's solo single released before military service?",
     {"Yours", "The Astronaut", "Epiphany", "Abyss"}, 1, QUIZ_MEDIUM},
    // 126
    {"Which BTS concert was held online due to COVID in 2020?",
     {"Bang Bang Con", "Map of the Soul ON:E", "Both A and B", "Permission to Dance"}, 2, QUIZ_MEDIUM},
    // 127
    {"What is the HYBE building's location in Seoul?",
     {"Gangnam", "Yongsan", "Itaewon", "Hongdae"}, 1, QUIZ_MEDIUM},
    // 128
    {"Which album features the song 'ON' with a marching band?",
     {"BE", "Map of the Soul: 7", "Love Yourself: Answer", "Wings"}, 1, QUIZ_MEDIUM},
    // 129
    {"What year did BTS win Top Duo/Group at Billboard Music Awards?",
     {"2019", "2021", "2022", "All of the above"}, 2, QUIZ_MEDIUM},
    // 130
    {"Which member's solo song from MOTS:7 is 'Filter'?",
     {"V", "Jimin", "Jungkook", "J-Hope"}, 1, QUIZ_MEDIUM},
    // 131
    {"What is Jungkook's solo song from MOTS:7?",
     {"Euphoria", "My Time", "Still With You", "Begin"}, 1, QUIZ_MEDIUM},
    // 132
    {"Which member hosted 'Suchwita', an interview web show?",
     {"RM", "Suga", "Jin", "V"}, 1, QUIZ_MEDIUM},
    // 133
    {"What platform originally hosted BTS's Bangtan Bombs?",
     {"Instagram", "YouTube", "V Live", "TikTok"}, 1, QUIZ_MEDIUM},
    // 134
    {"Which member is known for saying 'I purple you'?",
     {"Jimin", "Jungkook", "V", "J-Hope"}, 2, QUIZ_MEDIUM},
    // 135
    {"What does 'borahae' mean?",
     {"I love you", "I purple you", "I miss you", "Forever"}, 1, QUIZ_MEDIUM},
    // 136
    {"Which BTS song samples 'Lean on Me' by Bill Withers?",
     {"Life Goes On", "Spring Day", "Permission to Dance", "Dynamite"}, 0, QUIZ_MEDIUM},
    // 137
    {"What was BTS's first English-language single?",
     {"Butter", "Dynamite", "Permission to Dance", "Boy With Luv"}, 1, QUIZ_MEDIUM},
    // 138
    {"Which BTS member was the first to enlist in military service?",
     {"RM", "Suga", "Jin", "J-Hope"}, 2, QUIZ_MEDIUM},
    // 139
    {"What year did all BTS members complete their military service?",
     {"2024", "2025", "2026", "2027"}, 1, QUIZ_MEDIUM},

    // ═══════════════════════════════════════════════════════════
    // HARD (questions 140-199) — Deep cuts, production, records
    // ═══════════════════════════════════════════════════════════

    // 140
    {"What was BTS's pre-debut song released on SoundCloud?",
     {"Born Singer", "Path", "Outro: Circle Room Cypher", "We Are Bulletproof Pt. 1"}, 0, QUIZ_HARD},
    // 141
    {"Who is the main producer for BTS at BigHit?",
     {"Bang Si-hyuk", "Pdogg", "Slow Rabbit", "Supreme Boi"}, 1, QUIZ_HARD},
    // 142
    {"What was BTS's first music show win and on which show?",
     {"SBS Inkigayo for I Need U", "Mnet M Countdown for I Need U", "MBC Show! Music Core for Dope", "KBS Music Bank for Run"}, 1, QUIZ_HARD},
    // 143
    {"Which classical piece inspired the choreography of 'Black Swan'?",
     {"Tchaikovsky's Swan Lake", "Stravinsky's The Rite of Spring", "Debussy's Clair de Lune", "Vivaldi's Four Seasons"}, 0, QUIZ_HARD},
    // 144
    {"What was the name of RM's first solo mixtape (2015)?",
     {"Mono", "Indigo", "RM", "Namjoon"}, 2, QUIZ_HARD},
    // 145
    {"In which year did Suga release his first Agust D mixtape?",
     {"2015", "2016", "2017", "2018"}, 1, QUIZ_HARD},
    // 146
    {"What is the name of the fictional universe connecting BTS's MVs?",
     {"BTS Universe (BU)", "Bangtan World", "ARMY Universe", "Hwarang World"}, 0, QUIZ_HARD},
    // 147
    {"Which member wrote the lyrics for 'Tomorrow' from their debut era?",
     {"RM", "Suga", "J-Hope", "RM and Suga"}, 3, QUIZ_HARD},
    // 148
    {"What book by Hermann Hesse inspired BTS's 'Demian' concept?",
     {"Siddhartha", "Demian", "Steppenwolf", "Narcissus and Goldmund"}, 1, QUIZ_HARD},
    // 149
    {"What was BTS's trainee period before debut?",
     {"1 year", "2 years", "3 years", "Members varied (1-3 years)"}, 3, QUIZ_HARD},
    // 150
    {"Which BTS music video features a reference to Nietzsche?",
     {"Blood Sweat & Tears", "Spring Day", "Fake Love", "IDOL"}, 0, QUIZ_HARD},
    // 151
    {"What is the order of the Love Yourself album series?",
     {"Wonder, Her, Tear, Answer", "Her, Tear, Answer", "Tear, Her, Answer, Wonder", "Her, Wonder, Tear, Answer"}, 0, QUIZ_HARD},
    // 152
    {"Which song was originally a B-side but became one of BTS's most beloved songs?",
     {"Butterfly", "Spring Day", "Pied Piper", "Silver Spoon"}, 1, QUIZ_HARD},
    // 153
    {"What Guinness World Record did BTS set with 'Dynamite' MV?",
     {"Most viewed in 24 hours", "Most liked MV ever", "Fastest to 100M views", "Most comments"}, 0, QUIZ_HARD},
    // 154
    {"Which member originally trained to be a saxophone player?",
     {"RM", "V", "Suga", "J-Hope"}, 2, QUIZ_HARD},
    // 155
    {"What was the original concept for BTS before debuting as an idol group?",
     {"Dance crew", "Hip-hop group", "Rock band", "Ballad group"}, 1, QUIZ_HARD},
    // 156
    {"Which painting is referenced in 'Blood Sweat & Tears' MV?",
     {"Mona Lisa", "The Fall of the Rebel Angels", "Starry Night", "The Birth of Venus"}, 1, QUIZ_HARD},
    // 157
    {"What is the name of the outro track on 'Wings' that features all 7 members?",
     {"Interlude: Wings", "Outro: Wings", "2! 3!", "A Supplementary Story"}, 2, QUIZ_HARD},
    // 158
    {"Which BTS song was produced with Halsey before 'Boy With Luv'?",
     {"None - Boy With Luv was their first collab", "Waste It on Me", "MIC Drop remix", "Suga's interlude"}, 0, QUIZ_HARD},
    // 159
    {"What day is celebrated as BTS's official debut anniversary?",
     {"June 12", "June 13", "July 9", "March 2"}, 1, QUIZ_HARD},
    // 160
    {"Which member was the last to join BTS before debut?",
     {"V", "Jungkook", "J-Hope", "Jin"}, 0, QUIZ_HARD},
    // 161
    {"What is Suga's hometown basketball team that he often references?",
     {"Seoul Thunders", "Daegu BTS", "He references NBA, not KBL", "Daegu KOGAS Pegasus"}, 2, QUIZ_HARD},
    // 162
    {"Which member wrote a song about his pet sugar gliders?",
     {"Suga", "V", "Jin", "J-Hope"}, 2, QUIZ_HARD},
    // 163
    {"What was BTS's first sold-out stadium concert outside Asia?",
     {"Citi Field, New York", "Wembley Stadium, London", "Rose Bowl, LA", "MetLife Stadium, NJ"}, 0, QUIZ_HARD},
    // 164
    {"Which Agust D track samples a traditional Korean instrument?",
     {"Daechwita", "Agust D", "Give It to Me", "Strange"}, 0, QUIZ_HARD},
    // 165
    {"What literary work inspired 'The Notes' in BTS's storyline?",
     {"1984 by Orwell", "Demian by Hesse", "The Ones Who Walk Away from Omelas", "The Little Prince"}, 2, QUIZ_HARD},
    // 166
    {"Which member auditioned with a Rain song?",
     {"Jimin", "V", "Jungkook", "J-Hope"}, 2, QUIZ_HARD},
    // 167
    {"How many versions of 'We Are Bulletproof' exist?",
     {"1", "2", "3", "4"}, 2, QUIZ_HARD},
    // 168
    {"What was BTS's first daesang (grand prize) and at which show?",
     {"MMA 2016 Album of the Year", "MAMA 2016 Artist of the Year", "GDA 2017 Grand Prize", "Seoul Music Awards 2016"}, 1, QUIZ_HARD},
    // 169
    {"Which member composed 'Scenery' released on SoundCloud?",
     {"Jimin", "V", "Jungkook", "RM"}, 1, QUIZ_HARD},
    // 170
    {"What is the sample used in the intro of 'No More Dream'?",
     {"James Brown", "A helicopter sound", "A school bell", "A gunshot"}, 2, QUIZ_HARD},
    // 171
    {"Which member did a cover of 'We Don't Talk Anymore'?",
     {"V", "Jimin and Jungkook", "Jungkook", "Jin"}, 1, QUIZ_HARD},
    // 172
    {"What Jungian concept inspired 'Map of the Soul' series?",
     {"Shadow and Ego", "Persona, Shadow, and Ego", "Collective unconscious", "Archetypes only"}, 1, QUIZ_HARD},
    // 173
    {"Which BTS member appeared on the Korean variety show 'Problematic Men'?",
     {"Jin", "RM", "V", "Suga"}, 1, QUIZ_HARD},
    // 174
    {"What is the fan chant order for BTS members?",
     {"Kim Namjoon! Kim Seokjin! Min Yoongi! Jung Hoseok! Park Jimin! Kim Taehyung! Jeon Jungkook! BTS!", "Alphabetical by stage name", "By age, oldest first", "Random each concert"}, 0, QUIZ_HARD},
    // 175
    {"Which track from 'Dark & Wild' became a fan favorite choreography?",
     {"Danger", "War of Hormone", "Hip Hop Phile", "Let Me Know"}, 1, QUIZ_HARD},
    // 176
    {"What number does Suga wear on his jersey in choreographies?",
     {"3", "7", "13", "93"}, 0, QUIZ_HARD},
    // 177
    {"Which member's solo track 'Outro: Tear' won Best Recording Package at a Grammy nomination?",
     {"RM", "Suga", "It was the album, not a solo track", "J-Hope"}, 2, QUIZ_HARD},
    // 178
    {"What was the budget for BTS's debut reportedly?",
     {"$50,000", "$100,000", "Less than most K-pop debuts", "Over $1 million"}, 2, QUIZ_HARD},
    // 179
    {"Which member was street-cast while walking to school?",
     {"RM", "V", "Jin", "Jungkook"}, 2, QUIZ_HARD},
    // 180
    {"What dance competition show did J-Hope win as a child?",
     {"Hit the Stage", "JOY Dance Competition", "World of Dance", "Dancing 9"}, 1, QUIZ_HARD},
    // 181
    {"Which philosopher is referenced in 'Blood Sweat & Tears'?",
     {"Plato", "Kant", "Nietzsche", "Sartre"}, 2, QUIZ_HARD},
    // 182
    {"What is the hidden track on 'Love Yourself: Her'?",
     {"Sea", "Skit: Hesitation and Fear", "Her", "Outro: Her"}, 0, QUIZ_HARD},
    // 183
    {"Which member's audition was initially for a different company?",
     {"Jungkook (had multiple offers)", "V (came with a friend)", "Jimin (SM trainee)", "J-Hope (JYP trainee)"}, 0, QUIZ_HARD},
    // 184
    {"What is the hidden track on 'Love Yourself: Tear'?",
     {"Outro: Tear", "Skit: Hesitation and Fear", "Singularity", "Love Maze"}, 0, QUIZ_HARD},
    // 185
    {"How many weeks did 'Dynamite' spend on the Billboard Hot 100?",
     {"12 weeks", "22 weeks", "32 weeks", "42 weeks"}, 2, QUIZ_HARD},
    // 186
    {"Which member produced 'Blue Side' released on SoundCloud?",
     {"RM", "Suga", "J-Hope", "Jungkook"}, 2, QUIZ_HARD},
    // 187
    {"What year was BTS's online concert 'Bang Bang Con: The Live' held?",
     {"2019", "2020", "2021", "2022"}, 1, QUIZ_HARD},
    // 188
    {"Which BTS member played piano for 'First Love' live performances?",
     {"RM", "Jin", "Suga", "Jimin"}, 2, QUIZ_HARD},
    // 189
    {"What competition show chose Jungkook as a trainee after 7 companies offered?",
     {"Superstar K", "K-pop Star", "He chose BigHit after seeing RM", "Produce 101"}, 2, QUIZ_HARD},
    // 190
    {"Which song references the Sewol Ferry tragedy?",
     {"Spring Day", "Not Today", "Young Forever", "Whalien 52"}, 0, QUIZ_HARD},
    // 191
    {"What was the record-breaking viewership for 'Map of the Soul ON:E' online concert?",
     {"114,000", "756,000", "993,000", "1.3 million"}, 2, QUIZ_HARD},
    // 192
    {"Which member's birthday is March 9?",
     {"Suga", "J-Hope", "Jungkook", "V"}, 0, QUIZ_HARD},
    // 193
    {"What traditional Korean form inspired the choreography in 'IDOL'?",
     {"Buchaechum (fan dance)", "Samulnori drums", "Korean folk dance elements", "Taekwondo"}, 2, QUIZ_HARD},
    // 194
    {"Which member composed 'Abyss' and released it on his birthday?",
     {"Jimin", "V", "Jin", "RM"}, 2, QUIZ_HARD},
    // 195
    {"What was the record price for a 'Map of the Soul' tour ticket on resale?",
     {"$2,000", "$5,000", "$8,000+", "$15,000+"}, 2, QUIZ_HARD},
    // 196
    {"Which BTS track features only vocal line (no rap line)?",
     {"Spring Day", "Butterfly (prologue mix)", "Jamais Vu", "00:00"}, 2, QUIZ_HARD},
    // 197
    {"What was RM's IQ reported to be?",
     {"130", "138", "148", "152"}, 2, QUIZ_HARD},
    // 198
    {"Which BTS member was born in Ilsan, Gyeonggi-do?",
     {"Jin", "RM", "Suga", "J-Hope"}, 1, QUIZ_HARD},
    // 199
    {"What is Agust D spelled backwards?",
     {"D Tsuga (Suga reversed)", "DT Suga", "Suga TD (Suga's initials + Daegu Town)", "August Day"}, 0, QUIZ_HARD},
};

#ifdef __cplusplus
}
#endif

#endif // BTS_QUIZ_DATA_H
