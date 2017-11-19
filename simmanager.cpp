#include "simmanager.h"

//RJG - so can access MainWin
#include "mainwindow.h"

#include <QDebug>
#include <stdlib.h>
#include <math.h>
#include <QThread>
#include <QImage>
#include <QMessageBox>

//Simulation variables
quint32 tweakers[32]; // the 32 single bit XOR values (many uses!)
quint64 tweakers64[64]; // the 64 bit version
quint32 bitcounts[65536]; // the bytes representing bit count of each number 0-635535
quint32 xormasks[256][3]; //determine fitness
int xdisp[256][256];
int ydisp[256][256];
quint64 genex[65536];
int nextgenex;
quint64 cumulative_normal_distribution[33]; // RJG - A cumulative normal distribution for variable breeding.
quint64 reseedGenome=0; //RJG - Genome for reseed with known genome

//Settable ints
int gridX = 100;        //Can't be used to define arrays - hence both ATM
int gridY = 100;
int slotsPerSq = 100;
int startAge = 15;
int target = 66;
int settleTolerance = 15;
int dispersal = 15;
int food = 3000;
int breedThreshold = 500;
int breedCost = 500;
int maxDiff = 2;
int mutate = 10;
int envchangerate=100;
int yearsPerIteration=1;
int speciesSamples=1;
int speciesSensitivity=2;
int timeSliceConnect=5;
int lastReport=0;

//Settable bools
bool recalcFitness=false;
bool asexual=false;
bool variableBreed=false;
bool sexual=true;
bool speciesLogging=false;
bool speciesLoggingToFile=false;
bool fitnessLoggingToFile=false;
bool nonspatial=false;
bool toroidal=false;
bool reseedKnown=false;
bool reseedDual=false;
bool breedspecies=false, breeddiff=true;

//File handling
QStringList EnvFiles;
int CurrentEnvFile;
int EnvChangeCounter;
bool EnvChangeForward;
QString SpeciesLoggingFile="";
QString FitnessLoggingFile="";

//Globabl data
Critter critters[GRID_X][GRID_Y][SLOTS_PER_GRID_SQUARE]; //main array - static for speed
quint8 environment[GRID_X][GRID_Y][3];  //0 = red, 1 = green, 2 = blue
quint8 environmentlast[GRID_X][GRID_Y][3];  //Used for interpolation
quint8 environmentnext[GRID_X][GRID_Y][3];  //Used for interpolation
quint32 totalfit[GRID_X][GRID_Y];
quint64 generation;

//These next to hold the babies... old style arrays for max speed
quint64 newgenomes[GRID_X*GRID_Y*SLOTS_PER_GRID_SQUARE*2];
quint32 newgenomeX[GRID_X*GRID_Y*SLOTS_PER_GRID_SQUARE*2];
quint32 newgenomeY[GRID_X*GRID_Y*SLOTS_PER_GRID_SQUARE*2];
int newgenomeDisp[GRID_X*GRID_Y*SLOTS_PER_GRID_SQUARE*2];
quint64 newgenomespecies[GRID_X*GRID_Y*SLOTS_PER_GRID_SQUARE*2];
int newgenomecount;
quint8 randoms[65536];
quint16 nextrandom=0;

//Analysis
int breedattempts[GRID_X][GRID_Y]; //for analysis purposes
int breedfails[GRID_X][GRID_Y]; //for analysis purposes
int settles[GRID_X][GRID_Y]; //for analysis purposes
int settlefails[GRID_X][GRID_Y]; //for analysis purposes
int maxused[GRID_X][GRID_Y];
int AliveCount;

//Species stuff
QList<species> oldspecieslist;
QList< QList<species> > archivedspecieslists; //no longer used?
LogSpecies *rootspecies;
QHash<quint64,LogSpecies *> LogSpeciesById;
quint64 lastSpeciesCalc=0;
quint64 nextspeciesid;
QList<uint> species_colours;
quint8 species_mode;
quint64 ids; //used in tree export -
quint64 minspeciessize;
bool allowexcludewithissue;

QMutex *mutexes[GRID_X][GRID_Y]; //set up array of mutexes


SimManager::SimManager()
{
    //Constructor - set up all the data!
    species_mode=SPECIES_MODE_BASIC;
    MakeLookups();
    AliveCount=0;
    ProcessorCount=QThread::idealThreadCount();
    if (ProcessorCount==-1) ProcessorCount=1;
     if (ProcessorCount>256) ProcessorCount=256;  //a sanity check
    //ProcessorCount=1;
     for (int i=0; i<GRID_X; i++)
     for (int j=0; j<GRID_X; j++)
         mutexes[i][j]= new QMutex();

     for (int i=0; i<ProcessorCount; i++)
         FuturesList.append(new QFuture<int>);


     EnvFiles.clear();
     CurrentEnvFile=-1;
     EnvChangeCounter=0;
     EnvChangeForward=true;
     nextspeciesid=1;
     rootspecies=(LogSpecies *)0;
}


int SimManager::portable_rand()
{
    //replacement for qrand to come with RAND_MAX !=32767

    if (RAND_MAX<32767)
    {
        qDebug()<<"RAND_MAX too low - it's "<<RAND_MAX;
        exit(0);
    }
    if (RAND_MAX>32767)
    {
        // assume it's (2^n)-1
        int r = qrand();
        return r & 32767; //mask off bottom 16 bits, return those
    }
    else return qrand();
}

void SimManager::MakeLookups()
{
        //These are 00000001, 000000010, 0000000100 etc
        tweakers[0]=1;
        for (int n=1; n<32; n++) tweakers[n]=tweakers[n-1]*2;

        tweakers64[0]=1;
        for (int n=1; n<64; n++) tweakers64[n]=tweakers64[n-1]*2;

        //and now the bitcounting...
        // set up lookup 0 to 65535 to enable bits to be counted for each
        for (qint32 n=0; n<65536; n++)
        {
                qint32 count=0;
                for (int m=0; m<16; m++) if ((n & tweakers[m])!=0) ++count;  // count the bits
                bitcounts[n]=count;
        }

        //RJG - seed random from time qsrand(RAND_SEED);
        qsrand(QTime::currentTime().msec());

        //now set up xor masks for 3 variables - these are used for each of R G and B to work out fitness
        //Start - random bit pattern for each
        xormasks[0][0]=portable_rand() * portable_rand() *2;
        xormasks[0][1]=portable_rand() * portable_rand() *2;
        xormasks[0][2]=portable_rand() * portable_rand() *2;

        for (int n=1; n<256; n++) //for all the others - flip a random bit each time (^ is xor) - will slowly modify from 0 to 255
        {
                xormasks[n][0] = xormasks[n-1][0] ^ tweakers[portable_rand()/(PORTABLE_RAND_MAX/32)];
                xormasks[n][1] = xormasks[n-1][1] ^ tweakers[portable_rand()/(PORTABLE_RAND_MAX/32)];
                xormasks[n][2] = xormasks[n-1][2] ^ tweakers[portable_rand()/(PORTABLE_RAND_MAX/32)];
        }

        //now the randoms - pre_rolled random numbers 0-255
        for (int n=0; n<65536; n++) randoms[n] = (quint8)((portable_rand() & 255));
        nextrandom=0;

        // gene exchange lookup
        for (int n=0; n<65536; n++)  //random bit combs, averaging every other bit on
        {
                quint64 value = 0;
                for (int m=0; m<64; m++) if (portable_rand()>(PORTABLE_RAND_MAX/2)) value += tweakers64[m];
                genex[n]=value;
        }
        nextgenex=0;

        //dispersal table - lookups for dispersal amount
        //n is the distance to be dispersed - biased locally (the sqrt)
        //m is angle
        for (int n=0; n<256; n++)
        {
                double d=sqrt(65536/(double)(n+1))-16;
                if (d<0) d=0;
                for (int m=0; m<256; m++)
                {
                    xdisp[n][m]=(int)(d * sin((double)(m)/40.5845));
                    ydisp[n][m]=(int)(d * cos((double)(m)/40.5845));
                }
        }

        //colours
        for (int i=0; i<65536; i++)
        {
            species_colours.append(qRgb(Rand8(), Rand8(), Rand8()));
        }
}


void SimManager::loadEnvironmentFromFile(int emode)
// Load current envirnonment from file
{
    //Use make qimage from file method


    //Load the image
    if (CurrentEnvFile>=EnvFiles.count())
    {
        return;
    }
    QImage LoadImage(EnvFiles[CurrentEnvFile]);

    if (LoadImage.isNull())
    {
        QMessageBox::critical(0,"Error","Fatal - can't open image " + EnvFiles[CurrentEnvFile]);
        exit(1);
    }
    //check size works
    int xsize=LoadImage.width();
    int ysize=LoadImage.height();

    if (xsize<gridX || ysize<gridY) //rescale if necessary - only if too small
        LoadImage = LoadImage.scaled(QSize(gridX,gridY),Qt::IgnoreAspectRatio);

    //turn into environment array
    for (int i=0; i<gridX; i++)
    for (int j=0; j<gridY; j++)
    {
        QRgb colour = LoadImage.pixel(i,j);
        environment[i][j][0]=qRed(colour);
        environment[i][j][1]=qGreen(colour);
        environment[i][j][2]=qBlue(colour);
    }

    //set up environmentlast - same as environment
    for (int i=0; i<gridX; i++)
    for (int j=0; j<gridY; j++)
    {
        QRgb colour = LoadImage.pixel(i,j);
        environmentlast[i][j][0]=qRed(colour);
        environmentlast[i][j][1]=qGreen(colour);
        environmentlast[i][j][2]=qBlue(colour);
    }
    //set up environment next - depends on emode

    if (emode==0 || EnvFiles.count()==1) //static environment
    {
        for (int i=0; i<gridX; i++)
        for (int j=0; j<gridY; j++)
        {
            QRgb colour = LoadImage.pixel(i,j);
            environmentnext[i][j][0]=qRed(colour);
            environmentnext[i][j][1]=qGreen(colour);
            environmentnext[i][j][2]=qBlue(colour);
        }
    }
    else
    {
        //work out next file
        int nextfile;
        if (EnvChangeForward)
        {
            if ((CurrentEnvFile+1)<EnvFiles.count()) //not yet at end
                nextfile=CurrentEnvFile+1;
            else
            {
                //depends on emode
                if (emode==1) nextfile=CurrentEnvFile;//won't matter
                if (emode==2) nextfile=0; //loop mode
                if (emode==3) nextfile=CurrentEnvFile-1; //bounce mode
            }
        }
        else //backwards - simpler, must be emode 3
        {
            if (CurrentEnvFile>0) //not yet at end
                nextfile=CurrentEnvFile-1;
            else
                nextfile=1; //bounce mode
        }

        QImage LoadImage2(EnvFiles[nextfile]);
        if (xsize<gridX || ysize<gridY) //rescale if necessary - only if too small
            LoadImage2 = LoadImage2.scaled(QSize(gridX,gridY),Qt::IgnoreAspectRatio);
        //get it
        for (int i=0; i<gridX; i++)
        for (int j=0; j<gridY; j++)
        {
            QRgb colour = LoadImage2.pixel(i,j);
            environmentnext[i][j][0]=qRed(colour);
            environmentnext[i][j][1]=qGreen(colour);
            environmentnext[i][j][2]=qBlue(colour);
        }
    }
}

bool SimManager::regenerateEnvironment(int emode, bool interpolate)
//returns true if finished sim
{
    if (envchangerate==0 || emode==0 || EnvFiles.count()==1) return false; //constant environment - either static in menu, or 0 envchangerate, or only one file

    --EnvChangeCounter;

    if (EnvChangeCounter<=0)
    //is it time to do a full change?
    {
        if (emode!=3 && EnvChangeForward==false) //should not be going backwards!
            EnvChangeForward=true;
        if (EnvChangeForward)
        {
            CurrentEnvFile++; //next image
            if (CurrentEnvFile>=EnvFiles.count())
            {
                if (emode==1) return true;   //no more files and we are in 'once' mode - stop the sim
                if (emode==2) CurrentEnvFile=0; //loop mode
                if (emode==3)
                {
                    CurrentEnvFile-=2; //bounce mode - back two to undo the extra ++
                    EnvChangeForward=false;
                }
            }
        }
        else //going backwards - must be in emode 3 (bounce)
        {
            CurrentEnvFile--; //next image
            if (CurrentEnvFile<0)
            {
                CurrentEnvFile=1; //bounce mode - one to one again, must have just been 0
                EnvChangeForward=true;
            }
        }
        EnvChangeCounter=envchangerate; //reset counter
        loadEnvironmentFromFile(emode); //and load it from the file

    }
    else
    {
        if (interpolate)
        {
            float progress, invprogress;
            invprogress=((float)(EnvChangeCounter+1)/((float)envchangerate));
            progress=1-invprogress;
            //not getting new, doing an interpolate
            for (int i=0; i<gridX; i++)
            for (int j=0; j<gridY; j++)
            {
                environment[i][j][0]= qint8(0.5+((float)environmentlast[i][j][0]) * invprogress + ((float)environmentnext[i][j][0]) * progress);
                environment[i][j][1]= qint8(0.5+((float)environmentlast[i][j][1]) * invprogress + ((float)environmentnext[i][j][1]) * progress);
                environment[i][j][2]= qint8(0.5+((float)environmentlast[i][j][2]) * invprogress + ((float)environmentnext[i][j][2]) * progress);
            }
        }

    }
    return false;
}

quint32 SimManager::Rand32()
{
    //4 lots of RAND8
    quint32 rand1=portable_rand() & 255;
    quint32 rand2=(portable_rand() & 255) * 256;
    quint32 rand3=(portable_rand() & 255) * 256 * 256;
    quint32 rand4=(portable_rand() & 255) * 256 * 256 * 256;

    return rand1 + rand2 + rand3 + rand4;
}

quint8 SimManager::Rand8()
{
    return randoms[nextrandom++];
}

void SimManager::SetupRun()
{
    //Find middle square, try creatures till something lives, duplicate it [slots] times
    //RJG - called on initial program load and reseed, but also when run/run for are hit
    //RJG - with modification for dual seed if selected

    //Kill em all
    for (int n=0; n<gridX; n++)
    for (int m=0; m<gridY; m++)
    {
        for (int c=0; c<slotsPerSq; c++) {critters[n][m][c].age=0; critters[n][m][c].fitness=0;}
        totalfit[n][m]=0;
        maxused[n][m]=-1;
        breedattempts[n][m]=0;
        breedfails[n][m]=0;
        settles[n][m]=0;
        settlefails[n][m]=0;
    }

    nextspeciesid=1; //reset ID counter

    int n=gridX/2, m=gridY/2;
    int n2=0;

    //Temp dual seed
    if(reseedDual)
        {
        n=2;
        n2=gridX-2;
        }

    //RJG - Either reseed with known genome if set
    if(reseedKnown && !reseedDual)
                {
                    critters[n][m][0].initialise(reseedGenome,environment[n][m],n,m,0,nextspeciesid);
                    if (critters[n][m][0].fitness==0)
                        {
                            // RJG - But sort out if it can't survive...
                             QMessageBox::warning(0,"Oops","The genome you're trying to reseed with can't survive in this environment. There could be a number of reasons why this is. Please contact RJG or MDS to discuss.");
                             reseedKnown=false;
                             SetupRun();
                             return;
                        }

                    //RJG - I think this is a good thing to flag in an obvious fashion.
                    QString reseedGenomeString("Started simulation with known genome: ");
                    for (int i=0; i<64; i++)if (tweakers64[i] & reseedGenome) reseedGenomeString.append("1"); else reseedGenomeString.append("0");
                    MainWin->setStatusBarText(reseedGenomeString);
                }
    else if(reseedKnown && reseedDual)
                {
                    critters[n][m][0].initialise(reseedGenome,environment[n][m],n,m,0,nextspeciesid);
                    critters[n2][m][0].initialise(reseedGenome,environment[n2][m],n2,m,0,nextspeciesid);
                    if (critters[n][m][0].fitness==0||critters[n2][m][0].fitness==0)
                        {
                            // RJG - But sort out if it can't survive...
                             QMessageBox::warning(0,"Oops","The genome you're trying to reseed with can't survive in one of the two chosen environmental pixels. There could be a number of reasons why this is. Please contact RJG or MDS to discuss.");
                             reseedKnown=false;
                             SetupRun();
                             return;
                        }
                    //RJG - I think this is a good thing to flag in an obvious fashion.
                    QString reseedGenomeString("Started simulation with dual known genomes: ");
                    for (int i=0; i<64; i++)if (tweakers64[i] & reseedGenome) reseedGenomeString.append("1"); else reseedGenomeString.append("0");
                    MainWin->setStatusBarText(reseedGenomeString);
                }
    //RJG - or try till one lives. If alive, fitness (in critter file) >0
    else if(!reseedKnown && reseedDual)
                {
                    int flag=0;
                    do{
                        flag=0;
                        do critters[n][m][0].initialise((quint64)Rand32()+(quint64)(65536)*(quint64)(65536)*(quint64)Rand32(), environment[n][m], n,m,0,nextspeciesid);
                            while (critters[n][m][0].fitness<1);
                        quint64 gen=critters[n][m][0].genome;
                        critters[n2][m][0].initialise(gen, environment[n2][m],n2,m,0,nextspeciesid);
                        flag=critters[n2][m][0].fitness;
                        }while(flag<1);
                    MainWin->setStatusBarText("");
                }
    else
                {
                    while (critters[n][m][0].fitness<1) critters[n][m][0].initialise((quint64)Rand32()+(quint64)(65536)*(quint64)(65536)*(quint64)Rand32(), environment[n][m], n,m,0,nextspeciesid);
                    MainWin->setStatusBarText("");
                }

    totalfit[n][m]=critters[n][m][0].fitness; //may have gone wrong from above
    if(reseedDual)totalfit[n2][m]=critters[n2][m][0].fitness;

    AliveCount=1;
    quint64 gen=critters[n][m][0].genome;


    //HERE

    //Reset code to debug/play with variable breed.
    float x=-5., inc=(10./33);
    int cnt=0;
    quint64 cumulative_normal_distribution[33];
    do{

        float NSDF= (0.5 * erfc(-(x) * M_SQRT1_2));
        cumulative_normal_distribution[cnt]=4294967296*NSDF;

        qDebug()<<NSDF<<cnt<<cumulative_normal_distribution[cnt];
    x+=inc;
    cnt++;
    }while(cnt<33);

        for (int j=0;j<33;j++)
                {
            int asex=0, sex=0;
             for (int i=0;i<10000000;i++)
                {
                bool temp_asexual=false;
                //if(Rand32()>(4294967296/j))temp_asexual=true;
                //if(Rand32()>=(4294967296/tweakers64[j]))temp_asexual=true;
                //if(Rand32()>=(j*(4294967296/32)))temp_asexual=true;
                //if((Rand32()+Rand32())>=(j*(4294967296/32)))temp_asexual=true;
                //if((Rand32())>=((pow(10,j)/pow(10,32))*(4294967296/32)))temp_asexual=true;
                if(Rand32()>=cumulative_normal_distribution[j])temp_asexual=true;
                //if (Rand32()/(j+(4294967296/32)))>=1)
                //---- RJG: If asexual, recombine with self
                if(temp_asexual)asex++;
                else sex++;
                }
             float asexpct=((float)asex/10000000.)*100.;
             qDebug()<<"J is"<<j<<" Asex is "<<asex<<"; sex is"<<sex<<". So breeding is "<<asexpct<<"asexual.";
                }
/*
    for (int j=0;j<33;j++)
        {
        int ct=0;
        for (int i=0;i<10000000;i++)if (Rand32()<=tweakers64[j])ct++;
        float ct_av=((float)ct)/10000000.;
        qDebug()<<"J is: "<<j<<" Tweakers is: "<< tweakers64[j]<<"Count is: "<<ct<< "and the average is"<<ct_av;
        }


    //Sort out printing and counting here to test for variable breding
    qDebug()<<critters[n][m][0].genome<<" which is: ";

    QString genome_out;
    for (int j=0; j<32; j++)
        if (tweakers64[63-j] & critters[n][m][0].genome) genome_out.append("1"); else genome_out.append("0");
    genome_out.append(" ");
    for (int j=32; j<64; j++)
        if (tweakers64[63-j] & critters[n][m][0].genome) genome_out.append("1"); else genome_out.append("0");
    qDebug()<<genome_out;

    quint32 g1xu = quint32(critters[n][m][0].genome / ((quint64)65536*(quint64)65536)); //upper 32 bits
    quint32 t1 = bitcounts[g1xu/(quint32)65536] +  bitcounts[g1xu & (quint32)65535];

    qDebug()<<"And the count is"<<t1;

    quint32 lowergenome=(quint32)(critters[n][m][0].genome & ((quint64)65536*(quint64)65536-(quint64)1));

    qDebug()<<"Coding genome is: "<<lowergenome<<"which is: ";
    genome_out.clear();
    for (int j=32; j<64; j++)
        if (tweakers64[63-j] & lowergenome) genome_out.append("1"); else genome_out.append("0");
    qDebug()<<genome_out;*/

    //RJG - Fill square with successful critter
    for (int c=1; c<slotsPerSq; c++)
    {
        critters[n][m][c].initialise(gen, environment[n][m], n,m,c,nextspeciesid);
        if(reseedDual)critters[n2][m][c].initialise(gen, environment[n2][m], n2,m,c,nextspeciesid);

        if (critters[n][m][c].age>0)
        {
            critters[n][m][c].age/=((Rand8()/10)+1);
            critters[n][m][c].age +=10;
            AliveCount++;
            maxused[n][m]=c;
            totalfit[n][m]+=critters[n][m][c].fitness;
        }

        if(reseedDual && critters[n2][m][c].age>0)
        {
            critters[n2][m][c].age/=((Rand8()/10)+1);
            critters[n2][m][c].age +=10;
            AliveCount++;
            maxused[n2][m]=c;
            totalfit[n2][m]+=critters[n2][m][c].fitness;
        }
    }

    generation=0;

    EnvChangeCounter=envchangerate;
    EnvChangeForward=true;

    //remove old species log if one exists
    if (rootspecies) delete rootspecies;

    //create a new logspecies with appropriate first data entry
    rootspecies=new LogSpecies;

    rootspecies->maxsize=AliveCount;
    rootspecies->ID=nextspeciesid;
    rootspecies->time_of_first_appearance=0;
    rootspecies->time_of_last_appearance=0;
    rootspecies->parent=(LogSpecies *)0;
    LogSpeciesDataItem *newdata = new LogSpeciesDataItem;
    newdata->centroid_range_x=n;
    newdata->centroid_range_y=m;
    newdata->generation=0;
    newdata->cells_occupied=1;
    newdata->genomic_diversity=1;
    newdata->size=AliveCount;
    newdata->geographical_range=0;
    newdata->cells_occupied=0; //=1, this is stored as -1
    newdata->sample_genome=gen;
    newdata->max_env[0]=environment[n][m][0];
    newdata->max_env[1]=environment[n][m][1];
    newdata->max_env[2]=environment[n][m][2];
    newdata->min_env[0]=environment[n][m][0];
    newdata->min_env[1]=environment[n][m][1];
    newdata->min_env[2]=environment[n][m][2];
    newdata->mean_env[0]=environment[n][m][0];
    newdata->mean_env[1]=environment[n][m][1];
    newdata->mean_env[2]=environment[n][m][2];
    newdata->mean_fitness=(quint16)((totalfit[n][m]*1000)/AliveCount);

    rootspecies->data_items.append(newdata);
    LogSpeciesById.clear();
    LogSpeciesById.insert(nextspeciesid,rootspecies);

    oldspecieslist.clear();
    species newsp;
    newsp.ID=nextspeciesid;
    newsp.origintime=0;
    newsp.parent=0;
    newsp.size=slotsPerSq;
    newsp.logspeciesstructure=rootspecies;
    oldspecieslist.append(newsp);

    nextspeciesid++; //ready for first species after this

}

int SimManager::iterate_parallel(int firstx, int lastx, int newgenomecount_local, int *KillCount_local)
//parallel version - takes newgenomes_local as the start point it can write to in main genomes array
//returns number of new genomes
{
    int breedlist[SLOTS_PER_GRID_SQUARE];
    int maxalive;
    int deathcount;

    int asex=0, sex=0;

    for (int n=firstx; n<=lastx; n++)
    for (int m=0; m<gridY; m++)
    {
        int maxv=maxused[n][m];

        Critter *crit = critters[n][m];

        if (recalcFitness)
        {
            //Check it works

            //int oldtf=totalfit[n][m];
            totalfit[n][m]=0;
            maxalive=0;
            deathcount=0;
            for (int c=0; c<=maxv; c++)
            {
                if (crit[c].age)
                {
                    quint32 f=crit[c].recalc_fitness(environment[n][m]);
                    totalfit[n][m]+=f;
                    if (f>0) maxalive=c; else deathcount++;
                }
            }
            maxused[n][m]=maxalive;
            maxv=maxalive;
            (*KillCount_local)+=deathcount;
        }

        // RJG - reset counters for fitness logging to file
        if(fitnessLoggingToFile)breedattempts[n][m]=0;

        if (totalfit[n][m]) //skip whole square if needbe
        {
            int addfood = 1+(food / totalfit[n][m]);

            int breedlistentries=0;

            for (int c=0; c<=maxv; c++)
                    if (crit[c].iterate_parallel(KillCount_local,addfood)) breedlist[breedlistentries++]=c;


            // ----RJG: breedattempts was no longer used - co-opting for fitness report.
            if(fitnessLoggingToFile)breedattempts[n][m]=breedlistentries;

            if (breedlistentries>0)
            {
                quint8 divider=255/breedlistentries; //originally had breedlistentries+5, no idea why. //lol - RG
                for (int c=0; c<breedlistentries; c++)
                {
                    int partner;
                    bool temp_asexual=asexual;

                    //Here is where variable needs to be sorted.
                    if(variableBreed)
                        {
                        quint32 g1xu = quint32(crit[breedlist[c]].genome / ((quint64)65536*(quint64)65536)); //upper 32 bits
                        quint32 t1 = bitcounts[g1xu/(quint32)65536] +  bitcounts[g1xu & (quint32)65535];
                        if(Rand32()>=tweakers64[t1])temp_asexual=true;
                        else temp_asexual=false;
                        }

                    if(temp_asexual){partner=c;asex++;}
                    else {partner=Rand8()/divider;sex++;}

                    if (partner<breedlistentries)
                    {
                        if (crit[breedlist[c]].breed_with_parallel(n,m,&(crit[breedlist[partner]]),&newgenomecount_local))
                            breedfails[n][m]++; //for analysis purposes
                    }
                    else //didn't find a partner, refund breed cost
                        crit[breedlist[c]].energy+=breedCost;
                }
            }
        }
    }
    if(sex>0&&asex>0) qDebug()<<asex<<sex;
    return newgenomecount_local;
}

int SimManager::settle_parallel(int newgenomecounts_start, int newgenomecounts_end,int *trycount_local, int *settlecount_local, int *birthcounts_local)
{
    if (nonspatial)
    {
        //settling with no geography - just randomly pick a cell
        for (int n=newgenomecounts_start; n<newgenomecounts_end; n++)
        {
            quint64 xpos=((quint64)Rand32())*(quint64)gridX;
            xpos/=(((quint64)65536)*((quint64)65536));
            quint64 ypos=((quint64)Rand32())*(quint64)gridY;
            ypos/=(((quint64)65536)*((quint64)65536));

            mutexes[(int)xpos][(int)ypos]->lock(); //ensure no-one else buggers with this square
            (*trycount_local)++;
            Critter *crit=critters[(int)xpos][(int)ypos];
            //Now put the baby into any free slot here
            for (int m=0; m<slotsPerSq; m++)
            {
                Critter *crit2=&(crit[m]);
                if (crit2->age==0)
                {
                    //place it

                    crit2->initialise(newgenomes[n],environment[xpos][ypos], xpos, ypos,m,newgenomespecies[n]);
                    if (crit2->age)
                    {
                        int fit=crit2->fitness;
                        totalfit[xpos][ypos]+=fit;
                        (*birthcounts_local)++;
                        if (m>maxused[xpos][ypos]) maxused[xpos][ypos]=m;
                        settles[xpos][ypos]++;
                        (*settlecount_local)++;
                    }
                    else settlefails[xpos][ypos]++;
                    break;
                }
            }
            mutexes[xpos][ypos]->unlock();
        }
    }
    else
    {
        //old code - normal settling with radiation from original point
        //qDebug()<<"toroidal is "<<toroidal;
        for (int n=newgenomecounts_start; n<newgenomecounts_end; n++)
        {
            //first handle dispersal

            quint8 t1=Rand8();
            quint8 t2=Rand8();

            int xpos=(xdisp[t1][t2])/newgenomeDisp[n];
            int ypos=(ydisp[t1][t2])/newgenomeDisp[n];
            xpos+=newgenomeX[n];
            ypos+=newgenomeY[n];


            if (toroidal)
            {
                //NOTE - this assumes max possible settle distance is less than grid size. Otherwise it will go tits up
                if (xpos<0) xpos+=gridX;
                if (xpos>=gridX) xpos-=gridX;
                if (ypos<0) ypos+=gridY;
                if (ypos>=gridY) ypos-=gridY;
            }
            else
            {
                if (xpos<0) continue;
                if (xpos>=gridX)  continue;
                if (ypos<0)  continue;
                if (ypos>=gridY)  continue;
            }

            mutexes[xpos][ypos]->lock(); //ensure no-one else buggers with this square
            (*trycount_local)++;
            Critter *crit=critters[xpos][ypos];
            //Now put the baby into any free slot here
            for (int m=0; m<slotsPerSq; m++)
            {
                Critter *crit2=&(crit[m]);
                if (crit2->age==0)
                {
                    //place it

                    crit2->initialise(newgenomes[n],environment[xpos][ypos], xpos, ypos,m,newgenomespecies[n]);
                    if (crit2->age)
                    {
                        int fit=crit2->fitness;
                        totalfit[xpos][ypos]+=fit;
                        (*birthcounts_local)++;
                        if (m>maxused[xpos][ypos]) maxused[xpos][ypos]=m;
                        settles[xpos][ypos]++;
                        (*settlecount_local)++;
                    }
                    else settlefails[xpos][ypos]++;
                    break;
                }
            }
            mutexes[xpos][ypos]->unlock();

        }
    }
    return 0;
}


bool SimManager::iterate(int emode, bool interpolate)
{
    generation++;

    if (regenerateEnvironment(emode, interpolate)==true) return true;

    //New parallelised version

    int newgenomecounts_starts[256]; //allow for up to 256 threads
    int newgenomecounts_ends[256]; //allow for up to 256 threads

    //work out positions in genome array that each thread can write to to guarantee no overlap
    int positionadd=(GRID_X*GRID_Y*SLOTS_PER_GRID_SQUARE*2)/ProcessorCount;
    for (int i=0; i<ProcessorCount; i++)
        newgenomecounts_starts[i]=i*positionadd;

    int KillCounts[256];
    for (int i=0; i<ProcessorCount; i++) KillCounts[i]=0;


    //do the magic! Set up futures objects, call the functions, wait till done, retrieve values

    for (int i=0; i<ProcessorCount; i++)
        *(FuturesList[i]) = QtConcurrent::run(this, &SimManager::iterate_parallel, (i*gridX)/ProcessorCount, (((i+1)*gridX)/ProcessorCount)-1,newgenomecounts_starts[i], &(KillCounts[i]));

    for (int i=0; i<ProcessorCount; i++)
        FuturesList[i]->waitForFinished();

    for (int i=0; i<ProcessorCount; i++)
         newgenomecounts_ends[i]=FuturesList[i]->result();

    //Testbed - call parallel functions, but in series
/*    for (int i=0; i<ProcessorCount; i++)
        newgenomecounts_ends[i]=SimManager::iterate_parallel((i*gridX)/ProcessorCount, (((i+1)*gridX)/ProcessorCount)-1,newgenomecounts_starts[i], &(KillCounts[i]));
*/

    //apply all the kills to the global count
    for (int i=0; i<ProcessorCount; i++)
            AliveCount-=KillCounts[i];

    //Now handle spat settling

    int trycount=0;
    int settlecount=0;

    int trycounts[256]; for (int i=0; i<ProcessorCount; i++) trycounts[i]=0;
    int settlecounts[256]; for (int i=0; i<ProcessorCount; i++) settlecounts[i]=0;
    int birthcounts[256]; for (int i=0; i<ProcessorCount; i++) birthcounts[i]=0;

    //call the parallel settling function - in series for now
/*    for (int i=0; i<ProcessorCount; i++)
        settle_parallel(newgenomecounts_starts[i],newgenomecounts_ends[i],&(trycounts[i]), &(settlecounts[i]), &(birthcounts[i]));
*/

    //Parallel version of settle functions
    for (int i=0; i<ProcessorCount; i++)
        *(FuturesList[i]) = QtConcurrent::run(this, &SimManager::settle_parallel, newgenomecounts_starts[i],newgenomecounts_ends[i],&(trycounts[i]), &(settlecounts[i]), &(birthcounts[i]));

    for (int i=0; i<ProcessorCount; i++)
        FuturesList[i]->waitForFinished();

    //sort out all the counts
    for (int i=0; i<ProcessorCount; i++)
    {
         AliveCount+=birthcounts[i];
         trycount+=trycounts[i];
         settlecount+=settlecounts[i];
    }

    return false;
}

void SimManager::testcode()
//Use for any test with debugger, triggers from menu item
{
    qDebug()<<"Test code";

}


