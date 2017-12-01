#ifndef SIMMANAGER_H
#define SIMMANAGER_H
#include <QPlainTextEdit>
#include <QTime>
#include <QImage>
#include <QGraphicsPixmapItem>
#include <QMutex>
#include <QFuture>
#include <QStringList>

// ---- RJG updated this from QtConcurrent Aug16 to compile on linux
#include <QtConcurrentRun>

#include "critter.h"
#include "analyser.h"
#include "logspecies.h"

// Provides globals for the simulation - lookup tables for example
// Only one instance. Single instance - which is the only true global

//Some defines 
#define RAND_SEED 10000
#define PREROLLED_RANDS 60000

#define MAX_GENOME_COUNT 100000 //hopefully big enough for all species

//Settable ints
extern int gridX;        //Can't be used to define arrays
extern int gridY;
extern int slotsPerSq;
extern int startAge;

#define GRID_X 256
#define GRID_Y 256
#define SLOTS_PER_GRID_SQUARE 256

extern bool recalcFitness;
extern int target;
extern int settleTolerance;
extern int dispersal;

// multiplier determining how much food per square
extern int food;

//energy accumulation needed to breed
extern int breedThreshold;

//and cost of breeding - cost applies for success or failure of course
extern int breedCost;

//---- RJG: Also allow asexual and variable reproduction.
extern bool asexual;
extern bool variableBreed;
extern bool sexual;

//---- RJG: Pathogens on/off
extern bool path_on;

//----MDS: toroidal geography and non-spatial settling
extern bool nonspatial, toroidal;
extern bool breedspecies, breeddiff;

extern int maxDiff;
//chance to mutate out of 255
extern int mutate;
//Pathogen to mutate any given iteration out of 255
extern int path_mutate;

//Global lookups -
extern quint32 tweakers[32]; // the 32 single bit XOR values (many uses!)
extern quint64 tweakers64[64]; // 64-bit versions
extern quint32 bitcounts[65536]; // the bytes representing bit count of each number 0-635535
extern quint32 xormasks[256][3]; //determine fitness - just 32 bit (lower bit of genome)
extern int xdisp[256][256];
extern int ydisp[256][256];
extern quint64 genex[65536];
extern int nextgenex;
extern quint8 probbreed[65536][16];
extern quint8 randoms[65536];
extern quint16 nextrandom;
extern quint32 cumulative_normal_distribution[32]; // RJG - A cumulative normal distribution for variable breeding.
extern quint32 pathogen_prob_distribution[64];// RJG - A probability distribution for pathogens killing critters

//Globabl data
extern Critter critters[GRID_X][GRID_Y][SLOTS_PER_GRID_SQUARE]; //main array - static for speed
extern quint64 pathogens[GRID_X][GRID_Y]; //Pathogen overlay
extern quint8 environment[GRID_X][GRID_Y][3];  //0 = red, 1 = green, 2 = blue
extern quint8 environmentlast[GRID_X][GRID_Y][3];  //Used for interpolation
extern quint8 environmentnext[GRID_X][GRID_Y][3];  //Used for interpolation
extern quint32 totalfit[GRID_X][GRID_Y]; // ----RJG - Sum fitness critters in each square
extern quint64 generation;

//These next to hold the babies... old style arrays for max speed
extern quint64 newgenomes[GRID_X*GRID_Y*SLOTS_PER_GRID_SQUARE*2];
extern quint32 newgenomeX[GRID_X*GRID_Y*SLOTS_PER_GRID_SQUARE*2];
extern quint32 newgenomeY[GRID_X*GRID_Y*SLOTS_PER_GRID_SQUARE*2];
extern int newgenomeDisp[GRID_X*GRID_Y*SLOTS_PER_GRID_SQUARE*2];
extern quint64 newgenomespecies[GRID_X*GRID_Y*SLOTS_PER_GRID_SQUARE*2];
extern int newgenomecount;
extern int envchangerate;
extern int yearsPerIteration;
extern int speciesSamples; //no longer used - keep for backwards compat of files
extern int speciesSensitivity; //no longer used - keep for backwards compat of files
extern int timeSliceConnect; //no longer used - keep for backwards compat of files
extern bool speciesLogging;
extern bool speciesLoggingToFile;
extern QString SpeciesLoggingFile;
extern bool fitnessLoggingToFile;
extern QString FitnessLoggingFile;

extern QStringList EnvFiles;
extern int CurrentEnvFile;
extern quint64 lastSpeciesCalc;
extern int lastReport;

extern int breedattempts[GRID_X][GRID_Y]; //for analysis purposes
extern int breedfails[GRID_X][GRID_Y]; //for analysis purposes
extern int settles[GRID_X][GRID_Y]; //for analysis purposes
extern int settlefails[GRID_X][GRID_Y]; //for analysis purposes
extern int maxused[GRID_X][GRID_Y]; //---- RJG number of slots used within each grid square
extern int AliveCount;

//---- RJG reseed with known genomes
extern bool reseedKnown;
extern bool reseedDual;
extern quint64 reseedGenome;

extern int NextEnvChange;
extern int EnvChangeCounter;
extern bool EnvChangeForward;
extern QList<species> oldspecieslist;
extern QList< QList<species> > archivedspecieslists;
extern quint64 nextspeciesid;
extern LogSpecies *rootspecies;
extern QHash<quint64,LogSpecies *> LogSpeciesById;

extern QList<uint> species_colours;
extern quint8 species_mode;

#define SPECIES_MODE_NONE 0
#define SPECIES_MODE_BASIC 1
#define SPECIES_MODE_PHYLOGENY 2
#define SPECIES_MODE_PHYLOGENY_AND_METRICS 3

extern quint64 minspeciessize;
extern bool allowexcludewithissue;
extern quint64 ids;

//This is what we are aiming for overall for total bitcount
class SimManager
{
public:
    SimManager();

    void SetupRun();
    bool iterate(int emode, bool interpolate);
    int iterate_parallel(int firstx, int lastx, int newgenomes_local, int *KillCount_local);
    int settle_parallel(int newgenomecounts_start, int newgenomecounts_end,int *trycount_local, int *settlecount_local, int *birthcounts_local);

    quint8 Rand8();
    quint32 Rand32();
    quint64 Rand64();

    bool regenerateEnvironment(int emode, bool interpolate);
    void testcode();
    void loadEnvironmentFromFile(int emode);
    //Public data (keep public for speed)

    int portable_rand();

private:
    void MakeLookups();
    QList<QFuture<int>*> FuturesList;
    void debug_genome(quint64 genome);

    int ProcessorCount;

};

extern SimManager *TheSimManager;

#define PORTABLE_RAND_MAX 32767

#endif // SIMMANAGER_H
