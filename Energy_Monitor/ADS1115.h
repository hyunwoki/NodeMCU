Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */

//Maximum value of ADS
#define ADC_COUNTS 32768
#define PHASECAL 1.7
#define VCAL 0.8
#define ICAL 0.00835

double filteredI;
double filteredII;
double lastFilteredV,filteredV; //Filtered_ is the raw analog value minus the DC offset

int count=0;
int sampleV;                 //sample_ holds the raw analog read value
int sampleI; 
int sampleII; 

double offsetV;                          //Low-pass filter output
double offsetI;                          //Low-pass filter output
double offsetII; 
double realPower, apparentPower, powerFactor, Vrms, Irms, IrmsII;       
double phaseShiftedV; //Holds the calibrated phase shifted voltage.
int startV; //Instantaneous voltage at start of sample window.
double sqV,sumV,sqI,sqII,sumI, sumII,instP,sumP; //sq = squared, sum = Sum, inst = instantaneous
boolean lastVCross, checkVCross; //Used to measure number of times threshold is crossed.

double squareRoot(double fg)  
{
  double n = fg / 2.0;
  double lstX = 0.0;
  while (n != lstX)
  {
    lstX = n;
    n = (n + fg / n) / 2.0;
  }
  return n;
}

void calcVI(unsigned int crossings, unsigned int timeout)
{

  unsigned int crossCount = 0;                             //Used to measure number of times threshold is crossed.
  unsigned int numberOfSamples = 0;                        //This is now incremented  

  //-------------------------------------------------------------------------------------------------------------------------
  // 1) Waits for the waveform to be close to 'zero' (mid-scale adc) part in sin curve.
  //-------------------------------------------------------------------------------------------------------------------------
  boolean st=false;                                  //an indicator to exit the while loop

  unsigned long start = millis();    //millis()-start makes sure it doesnt get stuck in the loop if there is an error.

  while(st==false)                                   //the while loop...
  {
     startV = ads.readADC_Differential_2_3();                    //using the voltage waveform
     if ((abs(startV) < (ADC_COUNTS*0.55)) && (abs(startV) > (ADC_COUNTS*0.45))) st=true;  //check its within range
     if ((millis()-start)>timeout) st = true;
  }
  
  //-------------------------------------------------------------------------------------------------------------------------
  // 2) Main measurement loop
  //------------------------------------------------------------------------------------------------------------------------- 
  start = millis(); 

  while ((crossCount < crossings) && ((millis()-start)<timeout)) 
  {
    numberOfSamples++;                       //Count number of times looped.
    lastFilteredV = filteredV;               //Used for delay/phase compensation
    
    //-----------------------------------------------------------------------------
    // A) Read in raw voltage and current samples
    //-----------------------------------------------------------------------------
    sampleV = ads.readADC_Differential_2_3();                 //Read in raw voltage signal
    
    sampleII = ads.readADC_Differential_2_3();                 //Read in raw voltage signal    
    sampleI = ads.readADC_Differential_0_1();                 //Read in raw current signal

    //-----------------------------------------------------------------------------
    // B) Apply digital low pass filters to extract the 2.5 V or 1.65 V dc offset,
    //     then subtract this - signal is now centred on 0 counts.
    //-----------------------------------------------------------------------------
    offsetV = offsetV + ((sampleV-offsetV)/1024);
  filteredV = sampleV - offsetV;

    offsetI = offsetI + ((sampleI-offsetI)/1024);
  filteredI = sampleI - offsetI;
  
    offsetII = offsetII + ((sampleII-offsetII)/1024);
  filteredII = sampleII - offsetII;
   
    //-----------------------------------------------------------------------------
    // C) Root-mean-square method voltage
    //-----------------------------------------------------------------------------  
    sqV= filteredV * filteredV;                 //1) square voltage values
    sumV += sqV;                                //2) sum
    
    //-----------------------------------------------------------------------------
    // D) Root-mean-square method current
    //-----------------------------------------------------------------------------   
    sqI = filteredI * filteredI;                //1) square current values
    sumI += sqI;                                //2) sum 

    sqII = filteredII * filteredII;                //1) square current values
    sumII += sqII;                                //2) sum 
   
    //-----------------------------------------------------------------------------
    // E) Phase calibration
    //-----------------------------------------------------------------------------
    phaseShiftedV = lastFilteredV + PHASECAL * (filteredV - lastFilteredV); 
    
    //-----------------------------------------------------------------------------
    // F) Instantaneous power calc
    //-----------------------------------------------------------------------------   
    instP = phaseShiftedV * filteredI;          //Instantaneous Power
    sumP +=instP;                               //Sum  
    
    //-----------------------------------------------------------------------------
    // G) Find the number of times the voltage has crossed the initial voltage
    //    - every 2 crosses we will have sampled 1 wavelength 
    //    - so this method allows us to sample an integer number of half wavelengths which increases accuracy
    //-----------------------------------------------------------------------------       
    lastVCross = checkVCross;                     
    if (sampleV > startV) checkVCross = true; 
                     else checkVCross = false;
    if (numberOfSamples==1) lastVCross = checkVCross;                  
                     
    if (lastVCross != checkVCross) crossCount++;
  }
 
  //-------------------------------------------------------------------------------------------------------------------------
  // 3) Post loop calculations
  //------------------------------------------------------------------------------------------------------------------------- 
  //Calculation of the root of the mean of the voltage and current squared (rms)
  //Calibration coefficients applied. 
  float multiplier = 0.125F; /* ADS1115 @ +/- 4.096V gain (16-bit results) */
  double V_RATIO = VCAL * multiplier;
  Vrms = V_RATIO * squareRoot(sumV / numberOfSamples); 
  
  double I_RATIO = ICAL * multiplier;
  Irms = I_RATIO * squareRoot(sumI / numberOfSamples); 

  double I_RATIOII = ICAL * multiplier;
  IrmsII = I_RATIOII * squareRoot(sumII / numberOfSamples); 

  //Calculation power values
  realPower = V_RATIO * I_RATIO * sumP / numberOfSamples;
  apparentPower = Vrms * Irms;
  powerFactor=realPower / apparentPower;

  //Reset accumulators
  sumV = 0;
  sumI = 0;
  sumII = 0;
  sumP = 0;
//--------------------------------------------------------------------------------------       
}

double calcIrms(unsigned int Number_of_Samples)
{
  /* Be sure to update this value based on the IC and the gain settings! */
  float multiplier = 0.125F;    /* ADS1115 @ +/- 4.096V gain (16-bit results) */
  for (unsigned int n = 0; n < Number_of_Samples; n++)
  {
    sampleI = ads.readADC_Differential_0_1();
    sampleII = ads.readADC_Differential_2_3();

    // Digital low pass filter extracts the 2.5 V or 1.65 V dc offset, 
    //  then subtract this - signal is now centered on 0 counts.
    offsetI = (offsetI + (sampleI-offsetI)/1024);
    filteredI = sampleI - offsetI;

    //filteredI = sampleI * multiplier;
    offsetII = (offsetII + (sampleII-offsetII)/1024);
    filteredI = sampleII - offsetII;

    // Root-mean-square method current
    // 1) square current values
    sqI = filteredI * filteredI;
    sqII = filteredII * filteredII;
    // 2) sum 
    sumI += sqI;
    sumII += sqII;
  }
  
  Irms = squareRoot(sumI / Number_of_Samples)*multiplier; 
  IrmsII = squareRoot(sumII / Number_of_Samples)*multiplier; 

  //Reset accumulators
  sumI = 0;
//--------------------------------------------------------------------------------------       
 
  return Irms;
}
