

Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */

//Maximum value of ADS
#define ADC_COUNTS 32768
#define PHASECAL 1.7
#define VCAL 0.8
#define ICAL 0.00835

double filtered1;
double filtered2;
double lastFilteredV,filteredV; //Filtered_ is the raw analog value minus the DC offset

int count=0;
int sampleV;                 //sample_ holds the raw analog read value
int sample1; 
int sample2; 

double offsetV;                          //Low-pass filter output
double offset1;                          //Low-pass filter output
double offset2; 
double realPower, apparentPower, powerFactor, Vrms, Irms1, Irms2;       
double phaseShiftedV; //Holds the calibrated phase shifted voltage.
int startV; //Instantaneous voltage at start of sample window.
double sqV,sumV,sq1,sq2,sum1, sum2,instP,sumP; //sq = squared, sum = Sum, inst = instantaneous
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
    
    sample2 = ads.readADC_Differential_2_3();                 //Read in raw voltage signal    
    sample1 = ads.readADC_Differential_0_1();                 //Read in raw current signal

    //-----------------------------------------------------------------------------
    // B) Apply digital low pass filters to extract the 2.5 V or 1.65 V dc offset,
    //     then subtract this - signal is now centred on 0 counts.
    //-----------------------------------------------------------------------------
    offsetV = offsetV + ((sampleV-offsetV)/1024);
  filteredV = sampleV - offsetV;

    offset1 = offset1 + ((sample1-offset1)/1024);
  filtered1 = sample1 - offset1;
  
    offset2 = offset2 + ((sample2-offset2)/1024);
  filtered2 = sample2 - offset2;
   
    //-----------------------------------------------------------------------------
    // C) Root-mean-square method voltage
    //-----------------------------------------------------------------------------  
    sqV= filteredV * filteredV;                 //1) square voltage values
    sumV += sqV;                                //2) sum
    
    //-----------------------------------------------------------------------------
    // D) Root-mean-square method current
    //-----------------------------------------------------------------------------   
    sq1 = filtered1 * filtered1;                //1) square current values
    sum1 += sq1;                                //2) sum 

    sq2 = filtered2 * filtered2;                //1) square current values
    sum2 += sq2;                                //2) sum 
   
    //-----------------------------------------------------------------------------
    // E) Phase calibration
    //-----------------------------------------------------------------------------
    phaseShiftedV = lastFilteredV + PHASECAL * (filteredV - lastFilteredV); 
    
    //-----------------------------------------------------------------------------
    // F) Instantaneous power calc
    //-----------------------------------------------------------------------------   
    instP = phaseShiftedV * filtered1;          //Instantaneous Power
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
  
  double I_RATI1 = ICAL * multiplier;
  Irms1 = I_RATI1 * squareRoot(sum1 / numberOfSamples); 

  double I_RATI2 = ICAL * multiplier;
  Irms2 = I_RATI2 * squareRoot(sum2 / numberOfSamples); 

  //Calculation power values
  realPower = V_RATIO * I_RATI1 * sumP / numberOfSamples;
  apparentPower = Vrms * Irms1;
  powerFactor=realPower / apparentPower;


  //Reset accumulators
  sumV = 0;
  sum1 = 0;
  sum2 = 0;
  sumP = 0;
//--------------------------------------------------------------------------------------       
}

double calcIrms(unsigned int Number_of_Samples)
{
  /* Be sure to update this value based on the IC and the gain settings! */
  float multiplier = 0.125F;    /* ADS1115 @ +/- 4.096V gain (16-bit results) */
  for (unsigned int n = 0; n < Number_of_Samples; n++)
  {
    sample1 = ads.readADC_Differential_0_1();
    sample2 = ads.readADC_Differential_2_3();

    // Digital low pass filter extracts the 2.5 V or 1.65 V dc offset, 
    //  then subtract this - signal is now centered on 0 counts.
    offset1 = (offset1 + (sample1-offset1)/1024);
    filtered1 = sample1 - offset1;

    //filteredI = sampleI * multiplier;
    offset2 = (offset2 + (sample2-offset2)/1024);
    filtered2 = sample2 - offset2;

    // Root-mean-square method current
    // 1) square current values
    sq1 = filtered1 * filtered1;
    sq2 = filtered2 * filtered2;
    // 2) sum 
    sum1 += sq1;
    sum2 += sq2;
  }
  
  Irms1 = squareRoot(sum1 / Number_of_Samples)*multiplier; 
  Irms2 = squareRoot(sum2 / Number_of_Samples)*multiplier; 

  //Reset accumulators
  sum1 = 0;
  sum2 = 0;
//--------------------------------------------------------------------------------------       
 
  return Irms1, Irms2;
}
