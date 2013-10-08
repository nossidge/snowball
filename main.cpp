
/*//////////////////////////////////////////////////////////////////////////////

  Snowball Poem Generator

  by Paul Thompson - nossidge@gmail.com
  Project Email    - snowballpoetry@gmail.com
  Project Homepage - https://github.com/nossidge/snowball

//////////////////////////////////////////////////////////////////////////////*/

#define PROGRAM_VERSION "Version 1.41"
#define PROGRAM_DATE    "2013/10/08"

////////////////////////////////////////////////////////////////////////////////

// Windows file stuff
#include <windows.h>
#include <dirent.h>
#define GetCurrentDir _getcwd
#define PathSeparator "\\"

#include <getopt.h>

#include <algorithm>
#include <time.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#include <vector>
#include <map>

using namespace std;

string workingPath = "";
string programPath = "";
string programFile = "";

/// ////////////////////////////////////////////////////////////////////////////

// Output debug stuff to stdout?
//   (Won't output if outputIsQuiet)
bool outputIsVerbose = false;

// Should we print anything at all to stdout or stderr?
//   (overides the "outputIsVerbose" option)
bool outputIsQuiet = false;

// Output generated snowball poems to stdout INSTEAD OF to separate files.
//   (overides the "outputIsVerbose" option)
bool outputPoemsOnly = false;

// Is the input raw English text? (If not, then we'll use a file containing
//   previously discovered snowballing phrases)
bool processRawText = false;
string directoryRawInput = "";

// Should the temporary vectors be saved to disk as text files?
bool debugVectorsSaveToFile = false;

// The number of snowball poems to attempt to generate.
unsigned int poemTarget = 10000;

// How many failed poems to ignore before the program gives up.
// Only used with the -e option, or if seed phrases are used.
unsigned int poemFailureMax = 100000;

// Used for multi-key markov chains to dermine the percentage that the chosen
//   key will be used (as opposed to using a shorter, less context aware key).
unsigned int multiKeyPercentage = 70;

// Specify a minimum word key size to use. Default is 1.
unsigned int minKeySize = 1;

// Which method of snowball generator should we use.
//   Markov is more naturalistic‎, but random is sometimes interesting.
//   Only use "generatorRandom" if "minKeySize" equals 0.
bool generatorMarkov = true;
bool generatorRandom = false;

// Begin poems at a certain word length.
unsigned int poemWordBegin = 1;

// Reject poems where last word is fewer than 'n' letters long.
bool usePoemWordEnd = false;
unsigned int poemWordEnd = 8;

// Should we exclude any characters (for lipogram output).
bool excludeAnyChars = false;
string excludedChars = "";
unsigned int excludedCharMinWordLength = 0;

// Seed phrases. File name and the delimiter between each.
string seedPhrasesFileName = "";
char seedPhraseDelim = '\n';

// Use a lexicon file to validate words in the raw text files.
bool useLexiconFile = true;
string lexiconFileName = "snowball-lexicon.txt";

// The file to use to input preprocessed snowball phrases.
string preProcessedFileName = "snowball-preprocessed.txt";

// The file to use to interchange raw text input words.
bool useThesaurusFile = true;
string thesaurusFileName = "snowball-thesaurus.txt";

/// ////////////////////////////////////////////////////////////////////////////

// Vector for the lexicon. A simple word list.
vector<string> wordsLexicon;

/*  [keyWord],    [wordsThesaurus]
    {honor},      {honour}
    {honour},     {honor}
    {completely}, {utterly, totally}
    {utterly},    {totally, completely}
    {totally},    {completely, utterly}    */
map<string,vector<string> > wordsThesaurus;

/*  [wordLength], [wordsVector]
    {1},          {a, i, o}
    {2},          {it, am, to, do, we}
    {3},          {who, are, you, may}     */
map<int,vector<string> > wordsWithLength;

/*  [keyWord], [wordsForwards]
    {it},      {can, was, had}
    {am},      {the, our}
    {to},      {you, ask, the, put, say}
    {do|you},  {know, care, look}          */
map<string,vector<string> > wordsForwards;

/*  [keyWord], [wordsBackwards]
    {can},     {it, we}
    {the},     {is, do}                    */
map<string,vector<string> > wordsBackwards;

/// ////////////////////////////////////////////////////////////////////////////

// Don't output ever if (outputIsQuiet)
// Only output MSG_DEBUG stuff if (outputIsVerbose)
// If (outputPoemsOnly), only output MSG_POEMs
enum MsgType { MSG_STANDARD, MSG_ERROR, MSG_DEBUG, MSG_POEM };
void outputToConsole(string message, MsgType type) {
  if (outputPoemsOnly && (type == MSG_POEM) ) {
    cout << message << endl;
  } else if (!outputPoemsOnly && !outputIsQuiet) {
    if ( (type == MSG_STANDARD) || (type == MSG_DEBUG && outputIsVerbose) ) {
      cout << message << endl;
    } else if (type == MSG_ERROR) {
      cerr << message << endl;
    }
  }
}
void outputToConsoleMajorError() {
  ostringstream ss;
  ss <<
    "\n^^  This is a big yucky error!  ^^"
    "\nI haven't been able to replicate it, so I'm not sure how to fix it."
    "\nI'd be very grateful if you'd email me with details about this bug."
    "\nAny help would be enormously appreciated. Thank you."
    "\nEmail - snowballpoetry@gmail.com"
  ;
  outputToConsole(ss.str(),MSG_ERROR);
}
void outputToConsoleVersion(MsgType type) {
  ostringstream ss;
  ss <<
    "\n  Snowball Poem Generator - " PROGRAM_VERSION " - " PROGRAM_DATE
    "\n  by Paul Thompson - nossidge@gmail.com"
    "\n  Project Email    - snowballpoetry@gmail.com"
    "\n  Project Homepage - https://github.com/nossidge/snowball"
  ;
  outputToConsole(ss.str(),type);
}
void outputToConsoleUsage(MsgType type) {
  ostringstream ss;
  ss <<
    "\nUsage: snowball [-h | -V]"
    "\n       snowball [-v | -q] [-v | -o] [-d] [-n number] [-f number]"
    "\n                [-P number] [-k number] [-b number] [-e number]"
    "\n                [-x chars] [-X number] [-i (delim) | -s file]"
    "\n                [-p file] [-r directory] [l file | -L] [t file | -T]"
    "\n"
  ;
  outputToConsole(ss.str(),type);
}
void outputToConsoleHelp(MsgType type) {
  outputToConsoleVersion(type);
  outputToConsoleUsage(type);
  ostringstream ss;
  ss <<
      "Example: snowball -r input"
    "\n         snowball -s seed-phrases.txt"
    "\n         echo \"i am the,the only,the main,disqualified\" | snowball -i,"
    "\n"
    "\nProgram information:"
    "\n  -h        Output help instructions"
    "\n  -V        Output program information (version, author, contact info)"
    "\n"
    "\nConsole output:"
    "\n  Default   Write snowball file names to stdout, errors to stderr"
    "\n  -q        Do not write anything to standard or error output"
    "\n  -o        Write the snowball poems to stdout instead of to separate files"
    "\n  -v        Write verbose program information to standard output"
    "\n  -d        Debug. Write internal program vectors to separate files"
    "\n"
    "\nSnowball generation:"
    "\n                ( Arguments are all numeric integers )"
    "\n  -n10000       Specify how many snowballs to attempt to create"
    "\n  -f100000      Failed poems to ignore before just giving up"
    "\n  -P70          Percentage chance to use longer word keys before shorter keys"
    "\n  -k1           Specify a minimum word key size to use. Default is 1"
    "\n  -b4           Begin poems at a word length other than 1"
    "\n  -e8           Reject poems where last word is fewer than 'n' letters long"
    "\n  -x [ chars ]  Create poems that exclude all argument letters"
    "\n  -X3           Minimum word length to apply 'x' filtering to"
    "\n"
    "\nSnowball seed phrases:"
    "\n  -s filename.txt   Input seed phrases from a file, new line delimited"
    "\n  -i [ delim ]      Input seed phrases from stdin, 'delim' delimited"
    "\n"
    "\nProcessing raw input:"
    "\n  -p snowball-preprocessed.txt   Specify the preprocessed input file"
    "\n  -r ./input_directory           Create from raw English text files"
    "\n  -l snowball-lexicon.txt        The file that contains list of valid words"
    "\n  -L                             Don't use a lexicon file to validate words"
    "\n  -t snowball-thesaurus.txt      The file that contains the words to switch"
    "\n  -T                             Don't use a thesaurus file to switch words"
    "\n"
  ;
  outputToConsole(ss.str(),type);
}
/// ////////////////////////////////////////////////////////////////////////////
// Not sure if I'll need this, but it's good for debugging.
string toString(vector<string> &inputVector, string delimiter) {
  string returnString = "";
  for (vector<string>::iterator iter = inputVector.begin();
                                iter!=inputVector.end(); ++iter) {
    returnString = returnString + *iter + delimiter;
  }
  returnString.erase(returnString.find_last_not_of(delimiter)+1);
  return returnString;
}
/// ////////////////////////////////////////////////////////////////////////////
/// Split a string into a string vector
/// http://stackoverflow.com/questions/236129/splitting-a-string-in-c/236803#236803
vector<string> &split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}
vector<string> split(const string &s, char delim) {
    vector<string> elems;
    split(s, delim, elems);
    return elems;
}
/// ////////////////////////////////////////////////////////////////////////////
bool findInVector(vector<string> &haystack, string needle) {
  return (std::binary_search(haystack.begin(), haystack.end(), needle));
}
/// ////////////////////////////////////////////////////////////////////////////
void sortAndDedupe(vector<string> &inputVector) {
  vector<string>::iterator iter;
  std::sort (inputVector.begin(), inputVector.end());
  iter = std::unique (inputVector.begin(), inputVector.end());
  inputVector.resize( std::distance(inputVector.begin(),iter) );
}
void sortAndDedupe(map<int, vector<string> > &inputVector) {
  for(map<int, vector<string> >::iterator iter = inputVector.begin();
                                          iter != inputVector.end(); ++iter) {
    sortAndDedupe(iter->second);
  }
}
/// ////////////////////////////////////////////////////////////////////////////
void vectorSaveToFile(vector< vector<string> > &inputVector, string fileName,
                      bool appendToFile) {
  outputToConsole("vectorSaveToFile: " + fileName, MSG_DEBUG);

  ofstream outputFile;
  if (appendToFile) {
    outputFile.open(fileName.c_str(), fstream::app);
  } else {
    outputFile.open(fileName.c_str());
  }

  for(unsigned int i=0; i < inputVector.size(); i++) {
    for(unsigned int j=0; j < inputVector[i].size() ; j++) {
      outputFile << inputVector[i][j] << " ";
    } outputFile << endl;
  }
  outputFile.close();
}

void vectorSaveToFile(vector<string> &inputVector, string fileName,
                      bool appendToFile) {
  outputToConsole("vectorSaveToFile: " + fileName, MSG_DEBUG);

  ofstream outputFile;
  if (appendToFile) {
    outputFile.open(fileName.c_str(), fstream::app);
  } else {
    outputFile.open(fileName.c_str());
  }

  for(unsigned int i=0; i < inputVector.size(); i++) {
    outputFile << inputVector[i] << endl;
  }
  outputFile.close();
}
/// ////////////////////////////////////////////////////////////////////////////
// Saves all key/value combinations on a separate line, e.g.
// "i am"
// "i|am him"
// "i|am the"
// "i|am|the wind"
void mapSaveToFile(map<string, vector<string> > &inputVector,
                   string fileName) {
  outputToConsole("mapSaveToFile: " + fileName, MSG_DEBUG);

  ofstream outputFile;
  outputFile.open(fileName.c_str());
  for(map<string,vector<string> >::iterator iter = inputVector.begin();
                                            iter != inputVector.end();
                                            iter++) {
    vector<string> valuesList = iter->second;
    for (unsigned int i=0; i<valuesList.size(); i++) {
      outputFile << iter->first << " " << valuesList[i] << endl;
    }
  }
  outputFile.close();
}

// Saves each value on a separate line, with a header row for the key, e.g.
// "Key:    i"
// "Values: am"
// ""
// "Key:    i|am"
// "Values: him"
// "        the"
// ""
// "Key:    i|am|the"
// "Values: wind"
void mapSaveToFileKeyHeader(map<string, vector<string> > &inputVector,
                            string fileName) {
  outputToConsole("mapSaveToFileKeyHeader: " + fileName, MSG_DEBUG);

  ofstream outputFile;
  outputFile.open(fileName.c_str());
  for(map<string,vector<string> >::iterator iter = inputVector.begin();
                                            iter!= inputVector.end();
                                            iter++) {
    outputFile << "Key:    " << iter->first << endl;
    outputFile << "Values: ";
    vector<string> wordList = iter->second;
    for (unsigned int i=0; i<wordList.size(); i++) {
      outputFile << wordList[i] << endl << "        ";
    } outputFile << endl;
  }
  outputFile.close();
}

// Saves each value on a separate line, with a header row for the key, e.g.
// "Key:    1"
// "Values: a"
// "        i"
// "        o"
// ""
// "Key:    2"
// "Values: am"
// "        an"
// "        at"
void mapSaveToFileKeyHeader(map<int, vector<string> > &inputVector,
                            string fileName) {
  outputToConsole("mapSaveToFileKeyHeader: " + fileName, MSG_DEBUG);

  // Print the map to a file
  ofstream outputFile;
  outputFile.open(fileName.c_str());
  for(map<int,vector<string> >::iterator iter = inputVector.begin();
                                         iter!= inputVector.end();
                                         iter++) {
    outputFile << "Key:    " << iter->first << endl;
    outputFile << "Values: ";
    vector<string> wordList = iter->second;
    for (unsigned int i=0; i<wordList.size(); i++) {
      outputFile << wordList[i] << endl << "        ";
    } outputFile << endl;
  }
  outputFile.close();
}
/// ////////////////////////////////////////////////////////////////////////////
// Save all single word keys in {wordsBackwards} that are not
//   keys in {wordsForwards}
void saveToFileDeadBranches(string fileName) {
  outputToConsole("saveToFileDeadBranches: " + fileName, MSG_DEBUG);

  vector<string> deadBranches;

  for(map<string,vector<string> >::iterator iter = wordsBackwards.begin();
                                            iter != wordsBackwards.end();
                                            ++iter) {
    string theKey = iter->first;

    // Make sure it doesn't contain any pipe chars.
    std::size_t found = iter->first.find_first_of('|');
    if (found == std::string::npos) {
      if (wordsForwards[iter->first].size() == 0) {
        deadBranches.push_back(iter->first);
      }
    }
  }

  vectorSaveToFile(deadBranches,fileName,false);
}
/// ////////////////////////////////////////////////////////////////////////////
bool importLexicon(vector<string> &inputVector, string fileName) {
  outputToConsole("importLexicon: " + fileName, MSG_DEBUG);

  // Load the lexicon file. This contains a list of words, one per line.
  ifstream inputFile;
  inputFile.open(fileName.c_str());

  if (!inputFile.is_open()) {
    return false;

  } else {
    while (inputFile.good()) {

      string line;
      getline (inputFile,line);
      istringstream iss(line);

      do {

        // Examine each individual word.
        string word;
        iss >> word;
        if ( !word.empty() ) {

          // Make the word lowercase.
          std::transform(word.begin(), word.end(), word.begin(), ::tolower);

          wordsLexicon.push_back(word);
        }
      } while (iss) ;
    }
  }
  inputFile.close();

  sortAndDedupe(wordsLexicon);
  return true;
}
/// ////////////////////////////////////////////////////////////////////////////
/*  map<string,vector<string> > wordsThesaurus;
    [keyWord],    [wordsThesaurus]
    {honor},      {honour}
    {honour},     {honor}
    {completely}, {utterly, totally}
    {utterly},    {totally, completely}
    {totally},    {completely, utterly}    */
bool importThesaurus(map<string,vector<string> > &inputMap, string fileName) {
  outputToConsole("importThesaurus: " + fileName, MSG_DEBUG);

  // Load the thesaurus file.
  // This contains a list of words, grouped together on one line.
  ifstream inputFile;
  inputFile.open(fileName.c_str());

  if (!inputFile.is_open()) {
    return false;

  } else {
    while (inputFile.good()) {

      // Get the next line, and transform to lowercase.
      string line;
      getline(inputFile,line);
      std::transform(line.begin(), line.end(), line.begin(), ::tolower);

      // Split the line into separate words.
      vector<string> wordVector = split(line,' ');

      // Rotate the elements to get all possible variations.
      for (unsigned int i = 0; i < wordVector.size(); i++) {
        std::rotate(wordVector.begin(),wordVector.begin()+1,wordVector.end());

        // Load to inputMap.
        for (unsigned int j = 1; j < wordVector.size(); j++) {
          inputMap[wordVector[0]].push_back(wordVector[j]);
        }
      }
    }
  }
  inputFile.close();

  return true;
}
/// ////////////////////////////////////////////////////////////////////////////
// Transform an input string into a vector containing all its permutations,
//   as defined by the {wordsThesaurus} global vector.
//
// inputString: "The colors we view whilst dreaming"
//
// Output:
// The colors we view while dreaming
// The colors we view whilst dreaming
// The colours we view while dreaming
// The colours we view whilst dreaming
//
vector<string> lineExpandedFromThesaurus(string &inputString) {
  vector<string> phrases;

  // Just return inputString if we don't need to use the thesaurus.
  if (!useThesaurusFile) {
    phrases.push_back(inputString);

  } else {

    vector<string> phrasesOrig;

    // We loop through this later, so need to have one empty string element.
    phrases.push_back("");

    // Split into separate words.
    vector<string> words = split(inputString,' ');

    // Loop through the words.
    for (unsigned int iWord=0; iWord<words.size(); iWord++) {

      // See if the word is a key in the thesaurus map.
      unsigned int countThesaurusWords = wordsThesaurus[words[iWord]].size();
      bool found = ( countThesaurusWords != 0 );

      // If found, backup the current {phrases} vector.
      // We'll add to {phrases}, but read from {phrasesOrig}.
      if (found) phrasesOrig = phrases;

      // Even if not found, add the word to each phrase string.
      // Loop through each {phrase} and append to the end.
      for (unsigned int iPhrase=0; iPhrase<phrases.size(); iPhrase++) {
        phrases[iPhrase].append(words[iWord]+' ');
      }

      // If it is found, add ALL words that match the thesaurus key.
      if (found) {

        // For each phrase in {phrasesOrig}
        for (unsigned int iOrig=0; iOrig<phrasesOrig.size(); iOrig++) {
          string strPhrase = phrasesOrig[iOrig];

          // Loop through {phrasesOrig} and append to {phrases}.
          for (unsigned int i=0; i<countThesaurusWords; i++) {
            phrases.push_back(strPhrase+wordsThesaurus[words[iWord]][i]+' ');
          }
        }
      }
    }
  }

  return phrases;
}
/// ////////////////////////////////////////////////////////////////////////////
/// This function reads an unprocessed natural language text file (specified
///   in inputFileName), extracts just the word phrases that snowball upwards
///   in letter count, and outputs to a separate file whose name is returned
///   as the function's return value.
string loadInputFile(string inputFileName) {
  outputToConsole("loadInputFile:  Input: " + inputFileName, MSG_DEBUG);

  // We need to save each with a different file number suffix.
  // So we need to track how many files have been read.
  static unsigned int functionCounter = 0;
  functionCounter++;

  // Vector for the ascending length word strings.
  vector<string> rawSnowball;

  // Vector for words that are found in the input, but are not in the lexicon.
  vector<string> wordsNotInLexicon;

  // Loop through the raw input file.
  ifstream inputFile;
  inputFile.open(inputFileName.c_str());

  if (inputFile.is_open()) {
    while (inputFile.good()) {

      // Fill this up with word sequences that snowball.
      vector<string> snowballBuffer;

      // Get the next line, and transform to lowercase.
      string line;
      getline(inputFile,line);
      std::transform(line.begin(), line.end(), line.begin(), ::tolower);

      // Quick hack to fix memory issue when working with long lines.
      // Split line up into 5000 character strings. This might mean that we
      //   miss some possible snowballing phrases at the break of the lines,
      //   but it also means that we don't run out of memory.
      unsigned int const maxLen = 5000;
      unsigned int lineSize = line.size();
      vector<string> lineVectorOrig;
      for (unsigned int i = 0; i <= (lineSize / maxLen); i++) {
        string linePart = line.substr(i*maxLen,maxLen);
        lineVectorOrig.push_back(linePart);
      }

      // For each string in {lineVectorOrig}
      for (vector<string>::iterator iterLineOrig =lineVectorOrig.begin();
                                    iterLineOrig!=lineVectorOrig.end();
                                    ++iterLineOrig) {

        // Use the thesaurus to expand to all possible variants.
        //vector<string> lineVector = lineExpandedFromThesaurus(line);
        vector<string> lineVector = lineExpandedFromThesaurus(*iterLineOrig);

        // For each line (may be more than one because of the thesaurus)
        for (vector<string>::iterator iterLine =lineVector.begin();
                                      iterLine!=lineVector.end(); ++iterLine) {
          string previousWord;

          // Split the line into separate words.
          vector<string> wordVector = split(*iterLine,' ');

          // Examine each individual word.
          for (vector<string>::iterator iterWord =wordVector.begin();
                                        iterWord!=wordVector.end(); ++iterWord) {
            string word = *iterWord;

            // Default is true. Set to false if the word fails validation.
            bool validWord = true;

            // If the word contains punctuation it is invalid.
            unsigned int punctCount = 0;
            for (string::iterator it = word.begin(); it!=word.end(); ++it)
              if ( !isalpha(*it) ) ++punctCount;
            if (punctCount != 0) {
              validWord = false;
            }

            // Check the lexicon file, if necessary.
            if (useLexiconFile) {

              // If the word is not in the lexicon it is invalid.
              bool isInLexicon = findInVector(wordsLexicon,word);
              if ( validWord && !isInLexicon ) {
                validWord = false;

                // Write to {wordsNotInLexicon}, if necessary.
                if (debugVectorsSaveToFile) {
                  wordsNotInLexicon.push_back(word);
                }
              }
            }

            // If the lengths are separated by just one letter.
            bool validSnowball = ( (previousWord.length() != 0)
                 && (word.length() == previousWord.length()+1) );

            // If the word contains punctuation, then drop it.
            if (!validWord) word = "";

            // If we have come to an invalid word, or one that breaks the
            //   snowball sequence, then write whatever is in {snowballBuffer}
            //   to the
            if (!validSnowball || !validWord) {

              // Print out the contents of the {snowballBuffer} vector
              //   (but only if there's anything actually in it)
              if ( snowballBuffer.size() > 1 ) {
                stringstream ss;
                for (vector<string>::iterator iter = snowballBuffer.begin();
                                              iter!=snowballBuffer.end(); ++iter)
                    ss << *iter << " ";
                std::string s = ss.str();
                s.erase(s.find_last_not_of(" \n\r\t")+1);  // Right trim
                rawSnowball.push_back(s);
              }

              // Also, empty the {snowballBuffer} vector
              std::vector<string> empty;
              std::swap(snowballBuffer, empty);
            }

            // If the word is valid, add to {snowballBuffer}
            if (validWord && validSnowball) {

              // Add the word to the back of the {snowballBuffer} vector
              // (Also add previousWord, if it hasn't already been)
              if (snowballBuffer.size() == 0) {
                snowballBuffer.push_back(previousWord);
              }
              snowballBuffer.push_back(word);
            }
            previousWord = word;
          }
        }


        // If there's anything in {snowballBuffer}, copy to {rawSnowball}
        if ( snowballBuffer.size() > 1 ) {
          stringstream ss;
          for (vector<string>::iterator iter = snowballBuffer.begin();
                                        iter!= snowballBuffer.end(); ++iter)
              ss << *iter << " ";
          std::string s = ss.str();
          s.erase(s.find_last_not_of(" ")+1);
          rawSnowball.push_back(s);
        }
      }
    }
    inputFile.close();
  }

  // Write {wordsNotInLexicon} to a file, if necessary.
  if (useLexiconFile && debugVectorsSaveToFile) {
    sortAndDedupe(wordsNotInLexicon);
    vectorSaveToFile(wordsNotInLexicon,"output-wordsNotInLexicon.txt",true);
  }

  // Sort the raw snowball vector.
  sortAndDedupe(rawSnowball);

  // Save the raw snowball vector to a temporary "pro-" file.
  stringstream ss;
  ss << "pro-" << setw(5) << setfill('0') << functionCounter << ".txt";
  vectorSaveToFile(rawSnowball,ss.str(),true);

  // Output debug info to console.
  outputToConsole("loadInputFile: Output: " + ss.str(), MSG_DEBUG);

  // Return the name of the temporary "pro-" file.
  return ss.str();
}
/// ////////////////////////////////////////////////////////////////////////////
bool openInputPreprocessed(string fileName) {
  outputToConsole("openInputPreprocessed: " + fileName, MSG_DEBUG);

  vector<string> inputPreprocessed;

  // Loop through the raw input file.
  ifstream inputFile;
  inputFile.open(fileName.c_str());

  if (!inputFile.is_open()) {
    return false;

  } else {
    while (inputFile.good()) {

      string line;
      getline (inputFile,line);
      if ( !line.empty() ) {

        // Split line to many words
        vector<string> wordVector;
        wordVector = split(line,' ');

        // Fill wordsForwards and wordsBackwards
        string keyForwards, keyBackwards;
        for (unsigned int i = 0; i < wordVector.size(); i++) {
          if (i != wordVector.size()-1) {
            keyForwards = keyForwards + wordVector[i] + '|';
          }
          if (i != 0) {
            keyBackwards = keyBackwards + wordVector[i] + '|';
          }
        }
        keyForwards.erase(keyForwards.find_last_not_of("|")+1);
        keyBackwards.erase(keyBackwards.find_last_not_of("|")+1);

        string valueForwards = wordVector.back();
        string valueBackwards = wordVector.front();

        wordsForwards[keyForwards].push_back(valueForwards);
        wordsBackwards[keyBackwards].push_back(valueBackwards);

        // Also, populate {wordsWithLength} with only the first word.
        int wordLength = wordVector[0].length();
        wordsWithLength[wordLength].push_back(wordVector[0]);
      }
    }
  }
  inputFile.close();

  // Sort and dedupe the wordsWithLength map.
  sortAndDedupe(wordsWithLength);

  return true;
}
/// ////////////////////////////////////////////////////////////////////////////
bool loadInputFilesFromDirectory(string directoryPath, string fileNameOut) {
  outputToConsole("loadInputFilesFromDirectory: " + directoryPath, MSG_DEBUG);

  // We will loop through the root directory to load each file.
  // File stuff is pretty platform-specific. This works in Windows 7.
  // ( This is cribbed almost directly from
  //   http://stackoverflow.com/questions/612097 )
  vector<string> inputFiles;

  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir( directoryPath.c_str() )) != NULL) {

    // Print all the files and directories within the directory.
    while ( (ent = readdir(dir)) != NULL) {

      // Ignore dot-hidden files and current/parent directory refs.
      if ( ent->d_name[0] != '.' ) {
        inputFiles.push_back(ent->d_name);
      }
    }
    closedir(dir);
  } else { return false; }


  // Vector of all files that are created in this process.
  vector<string> allInputFiles;

  // Loop through {inputFiles} and load each using the loadInputFile function.
  for(vector<string>::iterator iter = inputFiles.begin();
                               iter != inputFiles.end();
                               iter++) {
    stringstream ssFullFilePath;
    ssFullFilePath << directoryPath << PathSeparator << *iter;
    string outputFileName = loadInputFile(ssFullFilePath.str().c_str());
    allInputFiles.push_back(outputFileName);
  }


  // Vector of all lines in all files.
  vector<string> allInputLines;

  // We now have a snowball output file for each input file in the directory.
  // Let's go through them, cat all them together and delete the originals.
  for (unsigned int i = 0; i < allInputFiles.size(); i++) {
    string currentFileName = allInputFiles[i];

    // Loop through the raw input file.
    ifstream inputFile;
    inputFile.open(currentFileName.c_str());
    if (inputFile.is_open()) {
      while (inputFile.good()) {
        string line;
        getline (inputFile,line);
        if (!line.empty()) {
          allInputLines.push_back(line);
        }
      }
    }
    inputFile.close();

    if( remove( currentFileName.c_str() ) != 0 ) {
      outputToConsole("Error deleting temporary file: " + currentFileName, MSG_ERROR);
      return false;
    } else {
      outputToConsole("Temporary file successfully deleted: " + currentFileName, MSG_DEBUG);
    }
  }

  // Sort and dedupe the {allInputLines} vector
  sortAndDedupe(allInputLines);


  /*
    {allInputLines} is full of lines like this:
      the lake
      the game title

    But we want to transform that into this:
      the lake
      game title
      the game title
      the game

    Something like the below AWK script:
    #!/bin/awk -f
    {
      for (i = NF; i > 0; i--) {
        s=$i
        for (j = i-1; j > 0; j--) {
          s=$j " " s
          print s
        }
      }
    }
  */
  outputToConsole("Generating processed input file", MSG_DEBUG);

  vector<string> inputPreprocessed;
  for (unsigned int k = 0; k < allInputLines.size(); k++) {
    vector<string> wordVector;
    wordVector = split(allInputLines[k],' ');
    for (int i = wordVector.size()-1; i > 0; i--) {
      string s = wordVector[i];
      for (int j = i-1; j >= 0; j--) {
        s = wordVector[j] + " " + s;
        inputPreprocessed.push_back(s);
      }
    }
  }
  sortAndDedupe(inputPreprocessed);
  vectorSaveToFile(inputPreprocessed,fileNameOut,false);

  return true;
}
/// ////////////////////////////////////////////////////////////////////////////
// Return a string vector that contains only the values of {inputVector} which
//   don't contain any of the chars in "charsToExclude"
vector<string> filterOutExcludedChars(vector<string> &inputVector,
                                      string charsToExclude) {
  vector<string> returnVector;
  string strToCheck;
  std::size_t forbiddenChars;

  // Loop through {inputVector} values.
  for(vector<string>::iterator iter = inputVector.begin();
                               iter != inputVector.end();
                               iter++) {

    strToCheck = (string)*iter;
    forbiddenChars = strToCheck.find_first_of(charsToExclude);

    // Add the value if there are no forbidden characters.
    if (forbiddenChars == std::string::npos) {
      returnVector.push_back(*iter);
    }
  }
  return returnVector;
}
// Return a string vector that contains only the valid values.
vector<string> validWords(vector<string> &inputVector) {
  if ( (!excludeAnyChars) || (inputVector.size() == 0) ) {
    return inputVector;
  } else {

    if (inputVector[0].length() >= excludedCharMinWordLength) {
      return filterOutExcludedChars(inputVector,excludedChars);
    } else {
      return inputVector;
    }
  }
}
/// ////////////////////////////////////////////////////////////////////////////
// Return a random element of a vector.
// Ensure it is valid using the (validWords) function.
// Return "" if no valid element.
string randomValidElement(vector<string> &inputVector) {
  string returnString;

  vector<string> valid = validWords(inputVector);
  if (valid.size() == 0) {
    returnString = "";
    outputToConsole("No valid element found.",MSG_DEBUG);
  } else {
    unsigned int randIndex = rand() % valid.size();
    returnString = valid[randIndex];
  }
  return returnString;
}
/// ////////////////////////////////////////////////////////////////////////////
// Snowball poem creator.
// string seedPhrase - Optional phrase to include in each snowball.
bool createPoemSnowball(string seedPhrase) {
  string fileName;

  // Only need the file name if it's going to be written to a file.
  if (!outputPoemsOnly) {

    // Add the seedPhrase to the file name, if necessary.
    string fileNameAppend = "";
    if (seedPhrase != "") { fileNameAppend = "-[" + seedPhrase + "]"; }
    stringstream ss;
    ss << "output-snowballPoems-" << time(NULL) << fileNameAppend << ".txt";

    // Set the fileName string and write debug info
    fileName = ss.str();
    outputToConsole("createPoemSnowball: " + fileName, MSG_DEBUG);
  }


  // It's entirely possible for the code to get to this point without any useful
  //   data in the input vectors. Do some basic checks to see if they're at
  //   least a little bit valid.
  if ( (wordsWithLength[1].size() == 0) || (wordsWithLength[2].size() == 0) ||
       (wordsForwards.size() == 0) || (wordsBackwards.size() == 0) ) {
    outputToConsole("Snowball input files contain invalid (or no) data.\n", MSG_ERROR);
    return false;
  }

  // Make sure the "poemWordBegin" value isn't too high to be useful.
  if ( wordsWithLength[poemWordBegin].size() == 0 ) {
    stringstream ss;
    ss << "Beginning word length \"" << poemWordBegin
       << "\" is too high for your input." << endl;
    outputToConsole(ss.str(), MSG_ERROR);
    return false;
  }

  // Make sure the "excludedChars" isn't too restrictive.
  if (excludeAnyChars) {

    unsigned int wordLengthToCheck = excludedCharMinWordLength;
    if (wordLengthToCheck == 0) {
      wordLengthToCheck = poemWordBegin;
    }

    if ( validWords(wordsWithLength[wordLengthToCheck]).size() == 0 ) {
      stringstream ss;
      ss << "The exclude characters string \"" << excludedChars
         << "\" is too restrictive for your input." << endl;
      outputToConsole(ss.str(), MSG_ERROR);
      return false;
    }
  }

  // If we are trying to find words between a begin length and an end length,
  //   make sure there are valid vectors for all possible word lengths.
  if (usePoemWordEnd || generatorRandom) {
    for (unsigned int i = poemWordBegin; i <= poemWordEnd; i++) {
      vector<string> checkVector = validWords(wordsWithLength[i]);
      if (checkVector.size() == 0) {
        stringstream ss;
        ss << "You have specified to generate poems from \"" << poemWordBegin
           << "\" letters long to \"" << poemWordEnd << "\" letters." << endl
           << "However, there are no words in the corpus that are \"" << i
           << "\" letters long." << endl;
        outputToConsole(ss.str(), MSG_ERROR);
        return false;
      }
    }
  }


  // Pre-load often used vectors. These will always be used to start off poems,
  //   so it makes sense to compute them now, and not again for each poem.
  vector<string> startingWords;
  if (seedPhrase == "") {
    startingWords = validWords(wordsWithLength[poemWordBegin]);
  }

  // Vector to hold all the generated snowballs.
  vector<string> allSnowballs;

  // Loop and loop to make lots of snowballs.
  unsigned int poemCountSuccess = 0;
  unsigned int poemCountFailure = 0;
  while ( (poemCountSuccess < poemTarget) && (poemCountFailure < poemFailureMax) ) {

    // Vector to hold all the current snowball.
    vector<string> theSnowball;
    string chosenWord;

    // Generate snowballs using markov chains.
    //   Using {wordsForwards} and {wordsBackwards}.
    if (generatorMarkov) {

      // seedPhrase does not exist.
      if (seedPhrase == "") {

        // Select a random word from {startingWords}
        chosenWord = randomValidElement(startingWords);
        theSnowball.push_back(chosenWord);

      // seedPhrase exists.
      } else {

        // There are two valid possibilities for seedPhrase.
        //   Type 1) Starting phrase: "i am new here"
        //   Type 2) Middle phrase: "songs soothe"
        //
        // Split seedPhrase on the space chars.
        // If seedPhrase[0].length() == 1, then it's Type 1.
        // We need to build up the snowball as normal, so that we can
        //   let the usual code fill out the rest.

        vector<string> seedPhraseVector;
        seedPhraseVector = split(seedPhrase,' ');
        chosenWord = seedPhraseVector[0];

        // Type 1) Starting phrase: "i am new here"
        if (chosenWord.length() == 1) {
          for (unsigned int i = 0; i < seedPhraseVector.size(); i++) {
            theSnowball.push_back(seedPhraseVector[i]);
          }

        // Type 2) Middle phrase: "songs soothe"
        } else {
          // We have a word or words from the middle of the snowball.
          // Fill in the start of the snowball backwards from there.
          // Once we've filled that in, we can break to the normal code.

          // Add the seed phrase to {theSnowball} vector.
          for (int i = seedPhraseVector.size()-1; i >= 0; i--) {
            theSnowball.push_back(seedPhraseVector[i]);
          }

          // If the first word is less than or equal to poemWordBegin
          //   (which defaults to 1), then don't look for smaller words.
          if ( !(seedPhraseVector[0].length() <= poemWordBegin) ) {

            bool wordNotMatched = false;
            do {
              unsigned int countOfMatched = validWords(wordsBackwards[chosenWord]).size();

              if (countOfMatched == 0) {
                wordNotMatched = true;
              } else {


                /// THOUGHT: This stuff is very similar to the Forward
                ///          Snowballs stuff below. Could this be gracefully
                ///          split out to a separate function?
                vector<string> allKeys;
                if (theSnowball.size() == 1) {
                  allKeys.push_back(chosenWord);
                } else {
                  for (unsigned int i=0; i<theSnowball.size(); i++) {
                    string aSingleKey;
                    for (unsigned int j=theSnowball.size(); j>i; j--) {
                      aSingleKey = aSingleKey + theSnowball[j-1] + "|";
                    } aSingleKey.erase(aSingleKey.find_last_not_of("|")+1);
                    allKeys.push_back(aSingleKey);
                  }
                }

                // Determine the longest key that returns a value.
                // Iterate forwards, use for loop so we can get the index.
                unsigned int iElement = 0;
                for (iElement = 0; iElement < allKeys.size(); iElement++) {
                  if (validWords(wordsBackwards[allKeys[iElement]]).size() != 0) break;
                }

                // We now know that all elements of {allKeys} from
                //   (iElement) to (allKeys.size()-1) are valid.
                // Loop through the valid elements to select the chosen key.

                // Default to the single word key.
                string chosenKey = allKeys[allKeys.size()-1];

                // Loop through the multi-word keys and randomly choose one.
                if (allKeys.size() > 1) {
                  for (unsigned int i = iElement; i < allKeys.size()-1; i++) {
                    unsigned int randNumber = rand() % 100 + 1;
                    // Use percentage variable to determine priority of the more
                    //   complex keys over the shorter ones.
                    if (randNumber <= multiKeyPercentage) {
                      chosenKey = allKeys[i];
                      break;
                    }
                  }
                }

                // Choose one of the values at random from the key.
                chosenWord = randomValidElement(wordsBackwards[chosenKey]);

                // Add the new word to the snowball vector.
                theSnowball.push_back(chosenWord);
                ///   ^ THOUGHT ^


              }
            } while (!wordNotMatched && chosenWord.length() > poemWordBegin);

            // If there was an error in the process, then abandon this poem.
            if (wordNotMatched) {
              poemCountFailure++;
              continue;
            }

            // Because we looped backwards, we now need to reverse the vector.
            std::reverse(theSnowball.begin(), theSnowball.end());

          }
        }

        chosenWord = seedPhraseVector.back();
      }


      // Find a random matching word in {wordsForwards}
      // Loop through the tree until it reaches a dead branch
      while (validWords(wordsForwards[chosenWord]).size() != 0) {
        // Calculate all possible keys, e.g.
        // Snowball      = "i am all cold"
        // Possible keys = "i|am|all|cold"
        //                   "am|all|cold"
        //                      "all|cold"
        //                          "cold"
        vector<string> allKeys;
        if (theSnowball.size() == 1) {
          allKeys.push_back(chosenWord);
        } else {
          for (unsigned int i=0; i<theSnowball.size(); i++) {
            string aSingleKey;
            for (unsigned int j=i; j<theSnowball.size(); j++) {
              aSingleKey = aSingleKey + theSnowball[j] + "|";
            } aSingleKey.erase(aSingleKey.find_last_not_of("|")+1);
            allKeys.push_back(aSingleKey);
          }
        }

        // Determine the longest key that returns a value.
        // Iterate forwards, use for loop so we can get the index.
        unsigned int iElement = 0;
        unsigned int invalidKeyCount = 0;
        for (iElement = 0; iElement < allKeys.size(); iElement++) {
          if (validWords(wordsForwards[allKeys[iElement]]).size() != 0) break;
          invalidKeyCount++;
        }

        // If the key is not big enough for minKeySize, then stop
        //   adding to the poem, and output what is there.
        if ( (allKeys.size() - invalidKeyCount) < minKeySize) {
          if (theSnowball.size() > minKeySize) {
            break;
          }
        }

        // We now know that all elements of {allKeys} from
        //   (iElement) to (allKeys.size()-1) are valid.
        // Loop through the valid elements to select the chosen key.

        // Default to the single word key.
        string chosenKey = allKeys[allKeys.size()-1];

        // Loop through the multi-word keys and randomly choose one.
        if (allKeys.size() > 1) {
          for (unsigned int i = iElement; i <= allKeys.size()-2; i++) {
            unsigned int randNumber = rand() % 100 + 1;

            // Use percentage variable to determine priority of the more
            //   complex keys over the shorter ones.
            if (randNumber <= multiKeyPercentage) {
              chosenKey = allKeys[i];
              break;
            }
          }
        }

        // Choose one of the values at random from the key.
        chosenWord = randomValidElement(wordsForwards[chosenKey]);

        // Add the new word to the snowball vector.
        theSnowball.push_back(chosenWord);
      }

      // If we require a poem to be at least a certain length, then
      //   the poem is considered a failure if it is too short.
      if (usePoemWordEnd) {
        unsigned int minimumWordCount = (1 + poemWordEnd - poemWordBegin);
        if ( theSnowball.size() < minimumWordCount ) {
          poemCountFailure++;
          continue;
        }
      }
    }

    // Generate snowballs using random correct-letter words.
    //   Using {wordsWithLength}.
    if (generatorRandom) {
      for (unsigned int i = poemWordBegin; i <= poemWordEnd; i++) {
        chosenWord = randomValidElement(wordsWithLength[i]);
        theSnowball.push_back(chosenWord);
      }
    }

    // Add {theSnowball} to {allSnowballs}.
    string snowballString;
    for (vector<string>::iterator iter = theSnowball.begin();
                                  iter!= theSnowball.end(); ++iter) {
      snowballString = snowballString + *iter + " ";
    } snowballString.erase(snowballString.find_last_not_of(" ")+1);
    allSnowballs.push_back(snowballString);

    // Add to the counter.
    poemCountSuccess++;
  }

  // Sort all the poems alphabetically.
  // This will also get rid of duplicates, although with a large
  //   preprocessed corpus file it's unlikely there will be many.
  sortAndDedupe(allSnowballs);

  // If the option "outputPoemsOnly" is true then:
  // Write the generated snowball poems to stdout INSTEAD OF to a file.
  if (outputPoemsOnly) {
    for(unsigned int i=0; i < allSnowballs.size(); i++) {
      outputToConsole(allSnowballs[i], MSG_POEM);
    }

  } else {

    // Open the output file for this batch.
    ofstream outputFileSingle;
    outputFileSingle.open (fileName.c_str(), fstream::app);

    // Print {allSnowballs} vector to output file.
    for (vector<string>::iterator iter = allSnowballs.begin();
                                  iter!= allSnowballs.end(); ++iter) {
      outputFileSingle << *iter << endl;
    }
    outputFileSingle.close();

    // Print the file name to stdout
    outputToConsole(fileName, MSG_STANDARD);
  }


  // If we had to abandon some poems due to multiple failures, inform the user.
  // But, since it's not really an error, only do this if the "verbose" option
  //   was specified (so use MSG_DEBUG).
  if (poemCountFailure >= poemFailureMax) {
    outputToConsole("Too many incomplete poems.", MSG_DEBUG);
    outputToConsole("Couldn't generate enough beginnings from seed phrase: "+seedPhrase, MSG_DEBUG);

    ostringstream ssConvertInt;
    ssConvertInt << "Target: " << poemTarget << " - Actual: " << poemCountSuccess;
    outputToConsole(ssConvertInt.str(), MSG_DEBUG);
  }

  return true;
}
/// ////////////////////////////////////////////////////////////////////////////
bool loadSeedPhrasesFromFile(vector<string> &inputVector, string fileName) {
  outputToConsole("loadSeedPhrasesFromFile: " + fileName, MSG_DEBUG);

  // Loop through the input file and fill to {inputVector}
  ifstream inputFile;
  inputFile.open(fileName.c_str());

  if (!inputFile.is_open()) {
    outputToConsole("Seed phrase file not found: " + fileName, MSG_ERROR);
    return false;

  } else {
    while (inputFile.good()) {

      string line;
      getline (inputFile,line);
      inputVector.push_back(line);
    }
  }
  inputFile.close();

  return true;
}
/// ////////////////////////////////////////////////////////////////////////////
// Windows only. Set the value of the "workingPath" global.
bool setPathWorkingDirectory() {
  char cCurrentPath[FILENAME_MAX];
  bool pathFound = GetCurrentDir(cCurrentPath, sizeof(cCurrentPath));
  workingPath = cCurrentPath;
  return pathFound;
}

// Windows only. Set the values of the "programPath" and "programFile" globals.
bool setPathAndFileName() {

  // Use Windows API GetModuleFileName() function.
  TCHAR pathOfEXEchar[2048];
  int bytes = GetModuleFileName(NULL, pathOfEXEchar, 2048);
  if (bytes == 0) return false;

  string pathOfEXE = pathOfEXEchar;

  // Separate the string to Path and File names.
  std::size_t found = pathOfEXE.find_last_of("/\\");
  bool pathSeparatorFound = (found != std::string::npos);

  // Set the variables if found.
  if (pathSeparatorFound) {
    programPath = pathOfEXE.substr(0,found);
    programFile = pathOfEXE.substr(found+1);
  }

  return pathSeparatorFound;
}
/// ////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[]) {

  // Find the location of the working directory.
  if (!setPathWorkingDirectory()) {
    outputToConsole("Could not determine working directory location.", MSG_ERROR);
    outputToConsoleMajorError();
    return EXIT_FAILURE;
  }

  // Find the location of the executable.
  if (!setPathAndFileName()) {
    outputToConsole("Could not determine program executable location.", MSG_ERROR);
    outputToConsoleMajorError();
    return EXIT_FAILURE;
  }

  // Default location for the files is the *executable* directory.
  //   (Not the working directory)
  lexiconFileName      = programPath + PathSeparator + lexiconFileName;
  preProcessedFileName = programPath + PathSeparator + preProcessedFileName;
  thesaurusFileName    = programPath + PathSeparator + thesaurusFileName;


  // String stream for use with output messages.
  ostringstream ss;

  // Booleans for whether or not the option was specified.
  bool invalidOption = false;
  bool opt_h = false, opt_V = false;
  bool opt_q = false, opt_o = false, opt_v = false, opt_d = false;
  bool opt_s = false, opt_i = false;
  bool opt_p = false, opt_r = false, opt_l = false, opt_L = false;
  bool opt_t = false, opt_T = false;

  // Handle option errors manually.
  extern int opterr; opterr = 0;

  // Loop through the argument list to determine which options were specified.
  int c;
  while ((c = getopt(argc, argv, ":hVqovdn:f:P:k:b:e:x:X:s:i::p:t:r:l:LT")) != -1) {
    switch (c) {

      // Options without arguments.
      case 'h':  opt_h = true;  break;
      case 'V':  opt_V = true;  break;
      case 'q':  opt_q = true;  break;
      case 'o':  opt_o = true;  break;
      case 'v':  opt_v = true;  break;
      case 'd':  opt_d = true;  break;
      case 'L':  opt_L = true;  break;
      case 'T':  opt_T = true;  break;

      // Specify how many snowballs to attempt to create.
      case 'n':
        poemTarget = (unsigned int)abs(atoi(optarg));
        break;

      // Specify how many failed poems to ignore.
      case 'f':
        poemFailureMax = (unsigned int)abs(atoi(optarg));
        break;

      // Specify the multi-key percentage.
      case 'P':
        multiKeyPercentage = (unsigned int)abs(atoi(optarg));
        break;

      // Specify the minimum word key size to use.
      case 'k':
        minKeySize = (unsigned int)abs(atoi(optarg));
        break;

      // Begin poems at a word length other than 1.
      case 'b':
        poemWordBegin = (unsigned int)abs(atoi(optarg));
        break;

      // Reject poems where last word is fewer than 'n' letters long.
      case 'e':
        usePoemWordEnd = true;
        poemWordEnd = (unsigned int)abs(atoi(optarg));
        break;

      // Characters to exclude.
      case 'x':
        excludeAnyChars = true;
        excludedChars = optarg;
        break;

      // Word length to begin 'x' filtering.
      case 'X':
        excludeAnyChars = true;
        excludedCharMinWordLength = (unsigned int)abs(atoi(optarg));
        break;

      // Take seed phrases as input from a file.
      case 's': // -s filename.txt
        opt_s = true;
        seedPhrasesFileName = optarg;
        break;

      // Take seed phrases as input from standard input
      case 'i': // -i delim
        opt_i = true;
        if (optarg != NULL) seedPhraseDelim = (char)optarg[0];
        break;

      // Specify the preprocessed input file.
      case 'p': // -p snowball-preprocessed.txt
        opt_p = true;
        preProcessedFileName = optarg;
        break;

      // Specify the thesaurus file.
      case 't': // -t snowball-thesaurus.txt
        opt_t = true;
        thesaurusFileName = optarg;
        break;

      // Create from raw text files.
      case 'r': // -r ./input_directory
        opt_r = true;
        directoryRawInput = optarg;
        break;

      // Specify the lexicon file.
      case 'l': // -l snowball-lexicon.txt
        opt_l = true;
        lexiconFileName = optarg;
        break;

      // Handle dodgy arguments.
      case ':':
        invalidOption = true;
        ss.str("");
        ss << "Option " << (char)optopt << " requires an argument.";
        outputToConsole(ss.str(), MSG_ERROR);
        break;


      // Handle dodgy cases.
      case '?':
        invalidOption = true;

        if (isprint(optopt)) {
          ss.str("");
          ss << "Unknown option: " << (char)optopt;
          outputToConsole(ss.str(), MSG_ERROR);

        } else {
          ss.str("");
          ss << "Unknown option character: " << (char)optopt;
          outputToConsole(ss.str(), MSG_ERROR);
        }
        break;

      default:
        invalidOption = true;
        outputToConsole("Something crazy happened with the options you specified.", MSG_ERROR);
    }
  }

  // Exit the program if it errored (We've already output MSG_ERROR messages)
  if (invalidOption) {
    outputToConsoleUsage(MSG_ERROR);
    return EXIT_FAILURE;
  }

  // Validation of inputs.
  if (poemWordBegin == 0) poemWordBegin = 1;
  if (poemWordEnd == 0) poemWordEnd = 1;
  if (poemWordEnd > 100) poemWordEnd = 100;
  if (multiKeyPercentage > 100) multiKeyPercentage = 100;
  if (minKeySize > 10) minKeySize = 10;
  if (minKeySize == 0) {
    generatorMarkov = false;  // If the key is 0, pick words at random
    generatorRandom = true;   //   instead of using Markov chains.
  }
  std::transform(excludedChars.begin(),   // Convert to lowercase.
                 excludedChars.end(),
                 excludedChars.begin(),
                 ::tolower);


  /// Handle the actual use of the options

  // -h  =  Print help, then exit the program
  if (opt_h) {
    outputToConsoleHelp(MSG_STANDARD);
    return EXIT_SUCCESS;
  }

  // -V  =  Print version of program, then exit the program
  if (opt_V) {
    outputToConsoleVersion(MSG_STANDARD);
    outputToConsole("", MSG_STANDARD);
    return EXIT_SUCCESS;
  }

  // Error if -l and -L
  if (opt_l && opt_L) {
    outputToConsole("You can't specify option -l as well as -L.", MSG_ERROR);
    outputToConsoleUsage(MSG_ERROR);
    return EXIT_FAILURE;
  }

  // Error if -t and -T
  if (opt_t && opt_T) {
    outputToConsole("You can't specify option -t as well as -T.", MSG_ERROR);
    outputToConsoleUsage(MSG_ERROR);
    return EXIT_FAILURE;
  }

  // Disable all messages; do not write anything to standard output.
  outputIsQuiet = opt_q;

  // Write the generated snowball poems to stdout INSTEAD OF to separate files.
  outputPoemsOnly = opt_o;

  // Write program information to standard output.
  outputIsVerbose = opt_v;

  // Write contents of internal program vectors to separate files.
  debugVectorsSaveToFile = opt_d;

  // Don't use a lexicon file.
  useLexiconFile = !opt_L;

  // Don't use a thesaurus file.
  useThesaurusFile = !opt_T;

  // There are 3 ways of handling seed phrases:
  //      Don't use a seed phrase at all.
  //   -s Read from specified file.
  //   -i Read from stdin.
  vector<string> seedPhrases;

  // Take seed phrases as input from a file.
  // Loop through the input file and fill {seedPhrases}
  if (opt_s) {
    if ( !loadSeedPhrasesFromFile(seedPhrases,seedPhrasesFileName) ) {
      outputToConsole("Require a valid file containing one Seed Phrase per line.", MSG_ERROR);
      return EXIT_FAILURE;
    }
  }

  // Take seed phrases as input from standard input.
  if (opt_i) {
    string line;
    while ( getline(cin,line,seedPhraseDelim) ) {
      line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
      seedPhrases.push_back(line);
    }
  }

  // Specify the preprocessed input file.
  // Check if "preProcessedFileName" is an existing file.
  // (Don't do this if we're creating a new one with -r)
  if (opt_p && !opt_r) {
    ifstream inputFile(preProcessedFileName.c_str());
    if ( !inputFile.good() ) {
      outputToConsole("Specified preprocessed file not found: " + preProcessedFileName, MSG_ERROR);
      return EXIT_FAILURE;
    }
  }

  // Create from raw text files.
  // Check if "directoryRawInput" is an existing directory.
  if (opt_r) {
    DIR *dir;
    if ((dir = opendir( directoryRawInput.c_str() )) == NULL) {
      outputToConsole("Specified input directory not found: " + directoryRawInput, MSG_ERROR);
      return EXIT_FAILURE;
    }
    processRawText = true;
  }

  // Debug state of the program due to the options.
  ss.str("");
  ss<< "\n  workingPath: "               << workingPath
    << "\n  programPath: "               << programPath
    << "\n  programFile: "               << programFile
    << "\n"
    << "\n  outputIsVerbose: "           << outputIsVerbose
    << "\n  outputIsQuiet: "             << outputIsQuiet
    << "\n  outputPoemsOnly: "           << outputPoemsOnly
    << "\n  processRawText: "            << processRawText
    << "\n  directoryRawInput: "         << directoryRawInput
    << "\n  debugVectorsSaveToFile: "    << debugVectorsSaveToFile
    << "\n  poemTarget: "                << poemTarget
    << "\n  poemFailureMax: "            << poemFailureMax
    << "\n  multiKeyPercentage: "        << multiKeyPercentage
    << "\n  poemWordBegin: "             << poemWordBegin
    << "\n  usePoemWordEnd: "            << usePoemWordEnd
    << "\n  poemWordEnd: "               << poemWordEnd
    << "\n  excludeAnyChars: "           << excludeAnyChars
    << "\n  excludedChars: "             << excludedChars
    << "\n  excludedCharMinWordLength: " << excludedCharMinWordLength
    << "\n  seedPhrasesFileName: "       << seedPhrasesFileName
    << "\n  useLexiconFile: "            << useLexiconFile
    << "\n  lexiconFileName: "           << lexiconFileName
    << "\n  preProcessedFileName: "      << preProcessedFileName
    << "\n  useThesaurusFile: "          << useThesaurusFile
    << "\n  thesaurusFileName: "         << thesaurusFileName
    << "\n"
  ;
  outputToConsole(ss.str(), MSG_DEBUG);


  /// Cool. All inputs and options dealt with.


  // Create the preprocessed file from raw text, if necessary.
  if (processRawText) {

    // Load the lexicon file if necessary.
    if (useLexiconFile) {
      if ( !importLexicon(wordsLexicon,lexiconFileName) ) {
        outputToConsole("Specified lexicon file not found: " + lexiconFileName, MSG_ERROR);
        outputToConsole("Check the file is valid, or use the -L option to disable lexicon checking.", MSG_ERROR);
        return EXIT_FAILURE;
      }
    }

    // Load the thesaurus file if necessary.
    if (useThesaurusFile) {
      if ( !importThesaurus(wordsThesaurus,thesaurusFileName) ) {
        outputToConsole("Specified thesaurus file not found: " + thesaurusFileName, MSG_ERROR);
        outputToConsole("Check the file is valid, or use the -T option to disable thesaurus substitution.", MSG_ERROR);
        return EXIT_FAILURE;
      }
    }

    // Read all input files from a chosen directory.
    // Save snowballing phrases to preProcessedFileName
    loadInputFilesFromDirectory(directoryRawInput,preProcessedFileName);
  }

  // Error if file is not found.
  if ( !openInputPreprocessed(preProcessedFileName) ) {
    outputToConsole("Cannot open preprocessed file: "+preProcessedFileName, MSG_ERROR);
    outputToConsole("This file is necessary for the program to function.", MSG_ERROR);
    outputToConsole("Please create this file and then run the program.", MSG_ERROR);
    outputToConsole("This file can be generated using the -r option.", MSG_ERROR);
    outputToConsole("You can use -h or check the readme for more info.", MSG_ERROR);
    return EXIT_FAILURE;
  }

  // Save the vectors as text files, if necessary
  if ( debugVectorsSaveToFile ) {
    mapSaveToFileKeyHeader(wordsForwards,"output-wordsForwards-keyHeader.txt");
    mapSaveToFile(wordsForwards,"output-wordsForwards.txt");
    mapSaveToFileKeyHeader(wordsBackwards,"output-wordsBackwards-keyHeader.txt");
    mapSaveToFile(wordsBackwards,"output-wordsBackwards.txt");
    mapSaveToFileKeyHeader(wordsWithLength,"output-wordsWithLength.txt");
    saveToFileDeadBranches("output-wordsDeadBranches.txt");
  }


  /// Inputs are all present and correct. Let's make some poems!


  // Set a seed for the RNG.
  srand (time(NULL));

  // True if it went wrong somewhere.
  bool problemWithTheSnowballs = false;

  // Create a poem from no seed phrase.
  if (seedPhrases.size() == 0) {
    if ( !createPoemSnowball("") ) {
      problemWithTheSnowballs = true;
    }

  // Loop through {seedPhrases} and create poems from those.
  } else {
    for (unsigned int i = 0; i < seedPhrases.size(); i++) {
      if ( !createPoemSnowball( seedPhrases[i] ) ) {
        problemWithTheSnowballs = true;
        break;
      }
    }
  }

  // Output if error.
  if (problemWithTheSnowballs) {
    ss.str("");
    ss << "Input data is invalid. Perhaps you used the wrong preprocessed file?"
      "\nOr maybe there just wasn't enough useful data in the file."
      "\nThis file can be generated using the -r option."
      "\nYou can use -h or check the readme for more info."
    ;
    outputToConsole(ss.str(),MSG_ERROR);
    return EXIT_FAILURE;
  }

  // If no error, then hooray!
  outputToConsole("\nreturn EXIT_SUCCESS", MSG_DEBUG);
  return EXIT_SUCCESS;
}
