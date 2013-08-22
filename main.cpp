
/*//////////////////////////////////////////////////////////////////////////////

  Snowball Poem Generator - Version 130822
  by Paul Thompson - snowballpoetry@gmail.com
  Homepage - https://github.com/nossidge/snowball

//////////////////////////////////////////////////////////////////////////////*/


// Windows file stuff
#define PathSeparator "\\"
#include <dirent.h>

#include <algorithm>
#include <time.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <stdio.h>

#include <cstring>
#include <string>
#include <vector>
#include <queue>
#include <map>

// Parse the program arguments
#include <unistd.h>
#include <getopt.h>

using namespace std;

/// ////////////////////////////////////////////////////////////////////////////

// Output debug stuff to stdout?
//   (Won't output if outputIsQuiet)
bool outputIsVerbose = false;

// Should we print anything at all to stdout or stderr?
//   (overides the "outputIsVerbose" option)
bool outputIsQuiet = false;

// Output generated snowball poems to stdout INSTEAD OF to separate files.
//   (overides the "outputIsVerbose" option)
bool outputIsPoems = false;

// Is the input raw English text? (If not, then we'll use a file containing
//   previously discovered snowballing phrases)
bool processRawText = false;
string directoryRawInput = "input";

// Should the temporary vectors be saved to disk as text files?
bool debugVectorsSaveToFile = false;

// Used for multi-key markov chains to dermine the percentage that the chosen
//   key will be used (as opposed to using a shorter, less context aware key).
unsigned int multiKeyPercentage = 70;

// The number of snowball poems to generate.
unsigned int poemTarget = 1000;

// This only comes into play when using a snowball "seed phrase" that is in
//   the middle of the snowball. If the program cannot find its way back to
//   a one letter word, then the poem will be ignored. This variable
//   determines how many failed poems to ignore before just giving up.
unsigned int poemFailureMax = 100000;

// Program option to read in seed phrases from a file. Default is false.
// If true, we also need the name of the input file.
string seedPhrasesFileName = "";
char seedPhraseDelim = '\n';

// Use a lexicon file to validate words in the raw text files.
bool useLexiconFile = true;
string lexiconFileName = "snowball-lexicon.txt";

// The file to use to input preprocessed snowball phrases.
string preProcessedFileName = "snowball-preprocessed.txt";

/// ////////////////////////////////////////////////////////////////////////////

// Vector for the lexicon. A simple word list.
vector<string> wordsLexicon;

/*  map<int,vector<string> >
    [wordLength], [wordsVector]
    {1},          {a, i, o}
    {2},          {it, am, to, do, we}
    {3},          {who, are, you, may}     */
map<int,vector<string> > wordsWithLength;

/*  map<string,vector<string> >
    [keyWord], [wordsForwards]
    {it},      {can, was, had}
    {am},      {the, our}
    {to},      {you, ask, the, put, say}
    {do|you},  {know, care, look}          */
map<string,vector<string> > wordsForwards;

/*  map<string,vector<string> >
    [keyWord], [wordsBackwards]
    {can},     {it, we}
    {the},     {is, do}                    */
map<string,vector<string> > wordsBackwards;

/// ////////////////////////////////////////////////////////////////////////////

// Don't output ever if (!outputIsQuiet)
// Only output DEBUG stuff if (outputIsVerbose)
#define STANDARD 1
#define ERROR 2
#define DEBUG 3
void outputToConsole(string message, int type) {
  if (!outputIsQuiet) {
    if ( (type == STANDARD) || (type == DEBUG && outputIsVerbose) ) {
      cout << message << endl;
    } else if (type == ERROR) {
      cerr << message << endl;
    }
  }
}
void outputToConsoleVersion(int type) {
  outputToConsole("", type);
  outputToConsole("  Snowball Poem Generator - Version 130822", type);
  outputToConsole("  by Paul Thompson - snowballpoetry@gmail.com", type);
  outputToConsole("  Homepage - https://github.com/nossidge/snowball", type);
}
void outputToConsoleUsage(int type) {
  ostringstream ss;
  ss <<
    "\nUsage: snowball [-h | -V]"
    "\n       snowball [-v | -q] [-v | -o] [-d] [-n number] [-f number] [-P number]"
    "\n                [-i (delim) | -s file] [-p file] [-r directory] [l file | -L]"
    "\n"
  ;
  outputToConsole(ss.str(),type);
}
void outputToConsoleHelp(int type) {
  outputToConsoleVersion(type);
  outputToConsoleUsage(type);
  ostringstream ss;
  ss <<
      "Example: snowball -r input"
    "\n         snowball -s seed-phrases.txt"
    "\n         echo \"i am the|the only|the main|disqualified\" | snowball -i\"|\""
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
    "\n  -n10000   Specify how many snowballs to attempt to create"
    "\n  -f100000  Failed poems to ignore before just giving up"
    "\n  -P70      Percentage chance to use multi-word keys before single-word keys"
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
    "\n"
  ;
  outputToConsole(ss.str(),type);
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
void vectorSortAndDedupe(vector<string> &inputVector) {
  vector<string>::iterator iter;
  std::sort (inputVector.begin(), inputVector.end());
  iter = std::unique (inputVector.begin(), inputVector.end());
  inputVector.resize( std::distance(inputVector.begin(),iter) );
}

void mapSortAndDedupe(map<int, vector<string> > &inputVector) {
  for(map<int, vector<string> >::iterator iter = inputVector.begin();
                                          iter != inputVector.end(); ++iter) {
    vectorSortAndDedupe(iter->second);
  }
}
/// ////////////////////////////////////////////////////////////////////////////
void vectorSaveToFile(vector< vector<string> > &inputVector, string fileName,
                      bool appendToFile) {
  outputToConsole("vectorSaveToFile: " + fileName, DEBUG);

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
  outputToConsole("vectorSaveToFile: " + fileName, DEBUG);

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
  outputToConsole("mapSaveToFile: " + fileName, DEBUG);

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
  outputToConsole("mapSaveToFileKeyHeader: " + fileName, DEBUG);

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
  outputToConsole("mapSaveToFileKeyHeader: " + fileName, DEBUG);

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
bool importLexicon(vector<string> &inputVector, string fileName) {
  outputToConsole("importLexicon: " + fileName, DEBUG);

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

  vectorSortAndDedupe(wordsLexicon);
  return true;
}
/// ////////////////////////////////////////////////////////////////////////////
void populateLengthMap(vector< vector<string> > &inputVector,
                       map<int,vector<string> > &outputMap,
                       string fileName) {

  outputToConsole("populateLengthMap: " + fileName, DEBUG);

  for(unsigned int i=0; i < inputVector.size() - 1; i++) {
    string firstWord = inputVector[i][0];
    int wordLength = firstWord.length();
    outputMap[wordLength].push_back(firstWord);
  }

  // Print the map to a file
  mapSaveToFileKeyHeader(outputMap,fileName);
}
/// ////////////////////////////////////////////////////////////////////////////
/// This function reads an unprocessed natural language text file (specified
///   in inputFileName), extracts just the word phrases that snowball upwards
///   in letter count, and outputs to a separate file whose name is returned
///   as the function's return value.
string loadInputFile(string inputFileName) {
  outputToConsole("loadInputFile:  Input: " + inputFileName, DEBUG);

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
      string previousWord;

      string line;
      getline (inputFile,line);
      istringstream iss(line);

      do {

        // Examine each individual word.
        string word;
        iss >> word;
        if (!word.empty()) {

          // Make the word lowercase.
          std::transform(word.begin(), word.end(), word.begin(), ::tolower);

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
              for (vector<string>::iterator iter = snowballBuffer.begin(); iter!=snowballBuffer.end(); ++iter)
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
      } while (iss) ;


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
    inputFile.close();
  }

  // Write {wordsNotInLexicon} to a file, if necessary.
  if (useLexiconFile && debugVectorsSaveToFile) {
    vectorSortAndDedupe(wordsNotInLexicon);
    vectorSaveToFile(wordsNotInLexicon,"output-wordsNotInLexicon.txt",true);
  }

  // Sort the raw snowball vector.
  vectorSortAndDedupe(rawSnowball);

  // Save the raw snowball vector to a temporary "pro-" file.
  stringstream ss;
  ss << "pro-" << setw(5) << setfill('0') << functionCounter << ".txt";
  vectorSaveToFile(rawSnowball,ss.str(),true);

  // Output debug info to console.
  outputToConsole("loadInputFile: Output: " + ss.str(), DEBUG);

  // Return the name of the temporary "pro-" file.
  return ss.str();
}
/// ////////////////////////////////////////////////////////////////////////////
bool openInputPreprocessed(string fileName) {
  outputToConsole("openInputPreprocessed: " + fileName, DEBUG);

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

        string valueForwards, valueBackwards;
        valueForwards = wordVector.back();
        valueBackwards = wordVector.front();

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
  mapSortAndDedupe(wordsWithLength);

  return true;
}
/// ////////////////////////////////////////////////////////////////////////////
bool loadInputFilesFromDirectory(string directoryPath, string fileNameOut) {
  outputToConsole("loadInputFilesFromDirectory: " + directoryPath, DEBUG);

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
      outputToConsole("Error deleting temporary file: " + currentFileName, ERROR);
      return false;
    } else {
      outputToConsole("Temporary file successfully deleted: " + currentFileName, DEBUG);
    }
  }

  // Sort and dedupe the {allInputLines} vector
  vectorSortAndDedupe(allInputLines);


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
  outputToConsole("Generating processed input file", DEBUG);

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
  vectorSortAndDedupe(inputPreprocessed);
  vectorSaveToFile(inputPreprocessed,fileNameOut,false);

  return true;
}
/// ////////////////////////////////////////////////////////////////////////////
// Snowball poem creator.
// string seedPhrase - Optional phrase to include in each snowball.
bool createPoemSnowball(string seedPhrase) {
  string fileName;

  // Only need the file name if it's going to be written to a file.
  if (!outputIsPoems) {

    // Add the seedPhrase to the file name, if necessary.
    string fileNameAppend = "";
    if (seedPhrase != "") { fileNameAppend = "-[" + seedPhrase + "]"; }
    stringstream ss;
    ss << "output-snowballPoems-" << time(NULL) << fileNameAppend << ".txt";

    // Set the fileName string and write debug info
    fileName = ss.str();
    outputToConsole("createPoemSnowball: " + fileName, DEBUG);
  }

  // It's entirely possible for the code to get to this point without any useful
  //   data in the input vectors. Do some basic checks to see if they're at
  //   least a little bit valid.
  if ( (wordsWithLength[1].size() == 0) || (wordsForwards.size() == 0) ||
       (wordsBackwards.size() == 0) ) {
    outputToConsole("Snowball input files contain invalid (or no) data.", ERROR);
    return false;
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
    unsigned int randIndex;

    // seedPhrase does not exist.
    if (seedPhrase == "") {

      // There are three approaches here.
      // Choose which one to use at random.
      unsigned int startMethod = rand() % 3 + 1;

      string oneLetterWords[3] = {"a","i","o"};

      switch (startMethod) {

        // Select a random 1 letter word from {wordsWithLength}
        case 1:
          randIndex = rand() % wordsWithLength[1].size();
          chosenWord = wordsWithLength[1][randIndex];
          break;

        // Just choose between one of A, I, or O
        case 2:
          randIndex = rand() % 3;
          chosenWord = oneLetterWords[randIndex];
          break;

        // Select a 2 letter word, and use "o" as the first line
        case 3:
          randIndex = rand() % wordsWithLength[2].size();
          chosenWord = wordsWithLength[2][randIndex];
          theSnowball.push_back("o");
          break;
      }
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

        bool wordNotMatched = false;
        do {
          unsigned int countOfMatched = wordsBackwards[chosenWord].size();

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
              for (int i=0; i<theSnowball.size(); i++) {
                string aSingleKey;
                for (int j=theSnowball.size()-1; j>=i; j--) {
                  aSingleKey = aSingleKey + theSnowball[j] + "|";
                } aSingleKey.erase(aSingleKey.find_last_not_of("|")+1);
                allKeys.push_back(aSingleKey);
              }
            }

            // Determine the longest key that returns a value.
            // Iterate forwards, use for loop so we can get the index.
            unsigned int iElement = 0;
            for (iElement = 0; iElement < allKeys.size(); iElement++) {
              if (wordsBackwards[allKeys[iElement]].size() != 0) break;
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
            randIndex = rand() % wordsBackwards[chosenKey].size();
            chosenWord = wordsBackwards[chosenKey][randIndex];

            // Add the new word to the snowball vector.
            theSnowball.push_back(chosenWord);
            ///   ^ THOUGHT ^


          }
        } while (!wordNotMatched && chosenWord.length() > 1);

        // If there was an error in the process, then abandon this poem.
        if (wordNotMatched) {
          poemCountFailure++;
          continue;
        }

        // Because we looped backwards, we now need to reverse the vector.
        std::reverse(theSnowball.begin(), theSnowball.end());
      }

      chosenWord = seedPhraseVector.back();
    }



    // Find a random matching word in {wordsForwards}
    // Loop through the tree until it reaches a dead branch
    while (wordsForwards[chosenWord].size() != 0) {
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
      for (iElement = 0; iElement < allKeys.size(); iElement++) {
        if (wordsForwards[allKeys[iElement]].size() != 0) break;
      }

      // We now know that all elements of {allKeys} from
      //   (iElement) to (allKeys.size()-1) are valid.
      // Loop through the valid elements to select the chosen key.

      // Default to the single word key.
      string chosenKey = allKeys[allKeys.size()-1];

      // Loop through the multi-word keys and randomly choose one.
      if (allKeys.size() > 1) {
        for (unsigned int i = iElement; i < allKeys.size()-2; i++) {
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
      randIndex = rand() % wordsForwards[chosenKey].size();
      chosenWord = wordsForwards[chosenKey][randIndex];

      // Add the new word to the snowball vector.
      theSnowball.push_back(chosenWord);
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

  // Sort the poems all alphabetically. (This will slso get rid
  //   of duplicates, although it's unlikely there will be any.)
  vectorSortAndDedupe(allSnowballs);


  // If the option "outputIsPoems" is true then:
  // Write the generated snowball poems to stdout INSTEAD OF to a file.
  if (outputIsPoems) {
    for(unsigned int i=0; i < allSnowballs.size(); i++) {
      outputToConsole(allSnowballs[i], STANDARD);
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
    outputToConsole(fileName, STANDARD);
  }


  // If we had to abandon some poems due to multiple failures, inform the user.
  // But, since it's not really an error, only do this if the "verbose" option
  //   was specified (so use DEBUG).
  if (poemCountFailure >= poemFailureMax) {
    outputToConsole("Too many incomplete poems.", DEBUG);
    outputToConsole("Couldn't generate enough beginnings from seed phrase: "+seedPhrase, DEBUG);

    ostringstream ssConvertInt;
    ssConvertInt << "Target: " << poemTarget << " - Actual: " << poemCountSuccess;
    outputToConsole(ssConvertInt.str(), DEBUG);
  }

  return true;
}
/// ////////////////////////////////////////////////////////////////////////////
bool loadSeedPhrasesFromFile(vector<string> &inputVector, string fileName) {
  outputToConsole("loadSeedPhrasesFromFile: " + fileName, DEBUG);

  // Loop through the input file and fill to {inputVector}
  ifstream inputFile;
  inputFile.open(fileName.c_str());

  if (!inputFile.is_open()) {
    outputToConsole("Seed phrase file not found: " + fileName, ERROR);
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
int main(int argc, char* argv[]) {

  // String stream for use with output messages.
  ostringstream ss;

  // Booleans for whether or not the option was specified.
  bool invalidOption = false;
  bool opt_h = false, opt_V = false;
  bool opt_q = false, opt_o = false, opt_v = false, opt_d = false;
  bool opt_s = false, opt_i = false;
  bool opt_p = false, opt_r = false, opt_l = false, opt_L = false;

  // Handle option errors manually.
  extern int opterr; opterr = 0;

  // Loop through the argument list to determine which options were specified.
  int c;
  while ((c = getopt(argc, argv, ":hVqovdn:f:Pi::p:r:l:L")) != -1) {
    switch (c) {

      // Options without arguments.
      case 'h':  opt_h = true;  break;
      case 'V':  opt_V = true;  break;
      case 'q':  opt_q = true;  break;
      case 'o':  opt_o = true;  break;
      case 'v':  opt_v = true;  break;
      case 'd':  opt_d = true;  break;
      case 'L':  opt_L = true;  break;

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
        if (multiKeyPercentage > 100) multiKeyPercentage = 100;
        break;

      // Take seed phrases as input from a file.
      case 's': // -s filename.txt
        opt_s = true;
        seedPhrasesFileName = optarg;
        break;

      // Specify the preprocessed input file.
      case 'p': // -p snowball-preprocessed.txt
        opt_p = true;
        preProcessedFileName = optarg;
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

      // Take seed phrases as input from standard input
      case 'i': // -i delim
        opt_i = true;
        if (optarg != NULL) seedPhraseDelim = (char)optarg[0];
        break;

      // Handle dodgy arguments.
      case ':':
        invalidOption = true;
        ss.str("");
        ss << "Option " << (char)optopt << " requires an argument.";
        outputToConsole(ss.str(), ERROR);
        break;


      // Handle dodgy cases.
      case '?':
        invalidOption = true;

        if (isprint(optopt)) {
          ss.str("");
          ss << "Unknown option: " << (char)optopt;
          outputToConsole(ss.str(), ERROR);

        } else {
          ss.str("");
          ss << "Unknown option character: " << (char)optopt;
          outputToConsole(ss.str(), ERROR);
        }
        break;

      default:
        invalidOption = true;
        outputToConsole("Something crazy happened with the options you specified.", ERROR);
    }
  }

  // Exit the program if it errored (We've already output ERROR messages)
  if (invalidOption) {
    outputToConsoleUsage(ERROR);
    return EXIT_FAILURE;
  }


  /// Handle the actual use of the options

  // -h  =  Print help, then exit the program
  if (opt_h) {
    outputToConsoleHelp(STANDARD);
    return EXIT_SUCCESS;
  }

  // -V  =  Print version of program, then exit the program
  if (opt_V) {
    outputToConsoleVersion(STANDARD);
    outputToConsole("", STANDARD);
    return EXIT_SUCCESS;
  }

  // Error if -l and -L
  if (opt_l && opt_L) {
    outputToConsole("You can't specify option -l as well as -L.", ERROR);
    outputToConsoleUsage(ERROR);
    return EXIT_FAILURE;
  }

  // Disable all messages; do not write anything to standard output.
  outputIsQuiet = opt_q;

  // Write the generated snowball poems to stdout INSTEAD OF to separate files.
  outputIsPoems = opt_o;

  // Write program information to standard output.
  outputIsVerbose = opt_v;

  // Write contents of internal program vectors to separate files.
  debugVectorsSaveToFile = opt_d;

  // Don't use a lexicon file.
  useLexiconFile = !opt_L;


  // There are 3 ways of handling seed phrases:
  //      Don't use a seed phrase at all.
  //   -s Read from specified file.
  //   -i Read from stdin.
  vector<string> seedPhrases;

  // Take seed phrases as input from a file.
  // Loop through the input file and fill {seedPhrases}
  if (opt_s) {
    if ( !loadSeedPhrasesFromFile(seedPhrases,seedPhrasesFileName) ) {
      outputToConsole("Require a valid file containing one Seed Phrase per line.", ERROR);
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
  if (opt_p) {
    ifstream inputFile(preProcessedFileName.c_str());
    if ( !inputFile.good() ) {
      outputToConsole("Specified preprocessed file not found: " + preProcessedFileName, ERROR);
      return EXIT_FAILURE;
    }
  }

  // Create from raw text files.
  // Check if "directoryRawInput" is an existing directory.
  if (opt_r) {
    DIR *dir;
    if ((dir = opendir( directoryRawInput.c_str() )) == NULL) {
      outputToConsole("Specified input directory not found: " + directoryRawInput, ERROR);
      return EXIT_FAILURE;
    }
    processRawText = true;
  }

  // Debug state of the program due to the options.
  ostringstream ssProgramStatus;
  ssProgramStatus << ">> outputIsVerbose: " << outputIsVerbose << endl;
  ssProgramStatus << ">> outputIsQuiet: " << outputIsQuiet << endl;
  ssProgramStatus << ">> outputIsPoems: " << outputIsPoems << endl;
  ssProgramStatus << ">> processRawText: " << processRawText << endl;
  ssProgramStatus << ">> directoryRawInput: " << directoryRawInput << endl;
  ssProgramStatus << ">> debugVectorsSaveToFile: " << debugVectorsSaveToFile << endl;
  ssProgramStatus << ">> multiKeyPercentage: " << multiKeyPercentage << endl;
  ssProgramStatus << ">> poemTarget: " << poemTarget << endl;
  ssProgramStatus << ">> poemFailureMax: " << poemFailureMax << endl;
  ssProgramStatus << ">> seedPhrasesFileName: " << seedPhrasesFileName << endl;
  ssProgramStatus << ">> useLexiconFile: " << useLexiconFile << endl;
  ssProgramStatus << ">> lexiconFileName: " << lexiconFileName << endl;
  ssProgramStatus << ">> preProcessedFileName: " << preProcessedFileName << endl;
  outputToConsole(ssProgramStatus.str(), DEBUG);


  /// Cool. All inputs and options dealt with.


  // Create the preprocessed file from raw text, if necessary.
  if (processRawText) {

    // Load the lexicon file if necessary.
    if (useLexiconFile) {
      if ( !importLexicon(wordsLexicon,lexiconFileName) ) {
        outputToConsole("Specified lexicon file not found: " + lexiconFileName, ERROR);
        outputToConsole("Check the file is valid, or use the -L option to disable lexicon checking.", ERROR);
        return EXIT_FAILURE;
      }
    }

    // Read all input files from a chosen directory.
    // Save snowballing phrases to preProcessedFileName
    loadInputFilesFromDirectory(directoryRawInput,preProcessedFileName);
  }

  // Error if file is not found.
  if ( !openInputPreprocessed(preProcessedFileName) ) {
    outputToConsole("Cannot open preprocessed file: "+preProcessedFileName, ERROR);
    outputToConsole("This file is necessary for the program to function.", ERROR);
    outputToConsole("Please create this file and then run the program.", ERROR);
    outputToConsole("This file can be generated using the -r option.", ERROR);
    outputToConsole("You can use -h or check the readme for more info.", ERROR);
    return EXIT_FAILURE;
  }

  // Save the vectors as text files, if necessary
  if ( debugVectorsSaveToFile ) {
    mapSaveToFileKeyHeader(wordsForwards,"output-wordsForwards-keyHeader.txt");
    mapSaveToFile(wordsForwards,"output-wordsForwards.txt");
    mapSaveToFileKeyHeader(wordsBackwards,"output-wordsBackwards-keyHeader.txt");
    mapSaveToFile(wordsBackwards,"output-wordsBackwards.txt");
    mapSaveToFileKeyHeader(wordsWithLength,"output-wordsWithLength.txt");
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
    outputToConsole("Input data is invalid. Perhaps you used the wrong preprocessed file?", ERROR);
    outputToConsole("Or maybe there just wasn't enough useful data in the file.", ERROR);
    outputToConsole("This file can be generated using the -r option.", ERROR);
    outputToConsole("You can use -h or check the readme for more info.", ERROR);
    return EXIT_FAILURE;
  }

  // If no error, then hooray!
  outputToConsole("return EXIT_SUCCESS", DEBUG);
  return EXIT_SUCCESS;
}
