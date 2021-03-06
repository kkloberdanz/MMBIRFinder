/*
 * programExecution.cpp
 *
 *  Created on: Jan 7, 2013
 *      Author: msegar
 */


#include "defs.h"
#include "globals.h"

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <stdlib.h>

#include <thread>
#include <mutex> 

using namespace std;

std::mutex mtx;

std::vector<std::string> sReadsFile_v;

void build_reads_sReadsFile_v(std::string sReadsFile) {
    size_t lastindex = sReadsFile.find_last_of("."); 
    std::string without_extension = sReadsFile.substr(0, lastindex); 
    for (unsigned int i = 0; i < NUM_THREADS; ++i) {
        sReadsFile_v.push_back(without_extension + ".part_" + std::to_string(i) + ".fastq");
        std::cout << "Pushed: " << without_extension + ".part_" + std::to_string(i) + ".fastq" << std::endl;
    }
}

void run_index_genome(std::string sReferenceFile) { 
    // First, we index the reference file
    if (executeBwaIndex(sReferenceFile) != 0)
        cout << "BWA index exited incorrectly" << endl;
}

void run_full_align(unsigned int threadnum, std::string sReferenceFile, std::string sReadsFile, std::string sOutputFile) {
    mtx.lock();
    if (threadnum)
        cout << "running thread: " << threadnum << endl;
    mtx.unlock();
    // Next, we run the BWA algorithm on FULL dataset reads
    if (executeBwaAligner(sReferenceFile, sReadsFile, (sOutputFile + "_1")) != 0)
        cout << "BWA aligner 1 exited incorrectly" << endl;
}

void run_convertSAMtoFASTA(unsigned int threadnum, std::string sUnalignedFile) {
    convertSAMtoFASTA(sUnalignedFile);
}

void run_filterOut(unsigned int threadnum, std::string sOutputFile, std::string sInputFile, std::string sFlag1, std::string sFlag2) {
    filterOut(sOutputFile, sInputFile, sFlag1, sFlag2);
}

void run_executeBwaAligner(unsigned int threadnum, std::string sReferenceFile, std::string sUnalignedFile, std::string sOutputFile) {
    executeBwaAligner(sReferenceFile,(sUnalignedFile), (sOutputFile));
}

void remove_header(std::string filename) {
    //std::string command = "sed -i '/^@/d' " + filename;
	/*
    std::string command = "sed -i -e 1,18d " + filename;
    cout << command << endl;
    system(command.c_str());
	*/

    mtx.lock();
    std::cout << "Removing header from: " << filename << std::endl;

    std::ifstream input;
    input.open(filename);
    //std::vector<std::string> whole_file;
    std::string   line = "";
    std::ofstream output;
    output.open(filename + ".tmp");

    mtx.unlock();
    bool keep_line = false;
    int i = 0;
    while (std::getline(input, line)) { 

        if (line[0] != '@') {
            keep_line = true;
        }

        if (keep_line) {
            output << line + '\n';
        }
    }

    mtx.lock();
    std::string command = "mv " + filename + ".tmp " + filename;
    system(command.c_str());
    mtx.unlock();
}

void run_get_reads(unsigned int threadnum,
                   std::string sUnalignedFile, 
                   std::string sOutputFile, 
                   std::string option1, 
                   std::string option2, 
                   bool b) {
    if (getReads(sUnalignedFile, sOutputFile, option1, option2, b) != 0)
        cout << "getReads 1 exited incorrectly" << endl;
}

void concatenate_to_file(std::vector<std::string> input_v, std::string output_filename) {
    std::string command = "rm -f " + output_filename;
    system(command.c_str());
    for (size_t i = 0; i < input_v.size(); ++i) { 

        if (i > 0) {
            remove_header(input_v.at(i));
        }
        std::string command = "cat " + input_v.at(i) + " >> " + output_filename;
        cout << command << endl;
        system(command.c_str());
        command = "rm -f " + input_v.at(i);
        system(command.c_str());
    } 
}

int startExecutables(int reads_file_index){

    cout << "Launched from thread: " << reads_file_index << endl;
    //string sReadsFile = sReadsFile_v[reads_file_index];
    cout << "\nStarting executables..." << endl;
    fLogFileOut << "\nStarting executables..." << endl;
    string sReferenceFile = confDB.getKey("referenceFile").stringVal; // the name of the reference file
    cout << "reference file: " << sReferenceFile << endl;


    string sReadsFile = confDB.getKey("readsFile").stringVal; // the name of the reads file

    std::cout << "Building vector using: " << sReadsFile << std::endl;
    build_reads_sReadsFile_v(sReadsFile);

    string sUnalignedFile = confDB.getKey("unalignedFile").stringVal; // the name of the unaligned FASTA/SAM file
    string sOutputFile = confDB.getKey("outputFile").stringVal; // name of the output file from BWA aligner
    // Debugging
    cout << "Writing BWA results to : '" << sOutputFile << "'" << endl;
    string sAlignedFile = confDB.getKey("alignedFile").stringVal; // name of the already aligned BAM file
    string sFinalAlignedFile = confDB.getKey("finalAlignedFile").stringVal;

    if (confDB.getKey("runAlignment").boolVal == true) {

        if (confDB.getKey("indexGenome").boolVal == true){
            // First, we index the reference file
            //if (executeBwaIndex(sReferenceFile) != 0)
                //cout << "BWA index exited incorrectly" << endl;
            cout << "running run_index_genome()" << endl;
            run_index_genome(sReferenceFile);
        }

        if (confDB.getKey("fullAlign").boolVal == true){
            // Next, we run the BWA algorithm on FULL dataset reads
            //if (executeBwaAligner(sReferenceFile, sReadsFile, (sOutputFile + "_1")) != 0)
                //cout << "BWA aligner 1 exited incorrectly" << endl;


            if (NUM_THREADS > 1) {
                cout << "Making threads" << endl;
                std::vector<std::thread> t;
                cout << "running run_full_align()" << endl;
                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    sReadsFile = sReadsFile_v[i];
                    t.push_back(thread(run_full_align, i, sReferenceFile, sReadsFile, sOutputFile + std::to_string(i)));
                } 

                // join threads here
                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    t[i].join();
                } 
                cout << "Threads rejoined" << endl;

                /*
                cout << "Concatenating output files back into one file" << endl; 

                {
                std::string command = "rm -f " + sProjectDirectory + sOutputFile + "_1.sam";
                system(command.c_str());
                } 

                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    if (i > 0) {
                        remove_header(sProjectDirectory + sOutputFile + std::to_string(i) + "_1.sam");
                    }
                    std::string command = "cat " + sProjectDirectory + sOutputFile + 
                                          std::to_string(i) + "_1.sam >> " + sProjectDirectory + 
                                          sOutputFile + "_1.sam"; 
                    cout << command << endl;
                    system(command.c_str());
                } 
                */
            } else {
                run_full_align(0, sReferenceFile, sReadsFile, sOutputFile);
            }
            sReadsFile = confDB.getKey("readsFile").stringVal; // the name of the reads file


            /* Debugging */
            //std::exit(EXIT_SUCCESS);

        }

        // TODO make the change to allow paired-end reads (need to remove -f 16 for getReads)

        // Then we get unaligned reads (flag of 0x4, 0x16)
        if (confDB.getKey("extractUnalignedReads").boolVal == true && confDB.getKey("bamFile").boolVal == false){

            cout << "running getReads()" << endl;
            if (NUM_THREADS > 1) {
                cout << "Making threads" << endl;
                std::vector<std::thread> t;
                cout << "running run_get_reads()" << endl;
                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    std::string str_i = std::to_string(i);
                    t.push_back(thread(run_get_reads, i, (sUnalignedFile + str_i + "_1"), (sOutputFile + str_i + "_1.sam"), "-f 4", "", true));
                } 

                // join threads here
                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    t[i].join();
                } 
                cout << "Threads rejoined" << endl;

            } else {
                run_full_align(0, sReferenceFile, sReadsFile, sOutputFile);
            }
            /*
            cout << "running getReads()" << endl;
            if (getReads((sUnalignedFile + "_1"), (sOutputFile + "_1.sam"), "-f 4", "", true) != 0)
                cout << "getReads 1 exited incorrectly" << endl;
            */

        } else if (confDB.getKey("extractUnalignedReads").boolVal == true && confDB.getKey("bamFile").boolVal == true){


            cout << "running getReads()" << endl;
            if (NUM_THREADS > 1) {
                cout << "Making threads" << endl;
                std::vector<std::thread> t;
                cout << "running run_get_reads()" << endl;
                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    std::string str_i = std::to_string(i);
                    t.push_back(thread(run_get_reads, i, (sUnalignedFile + str_i + "_1"), (sAlignedFile + str_i), "-f 4", "", true));
                } 

                // join threads here
                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    t[i].join();
                } 
                cout << "Threads rejoined" << endl;

            } else {
                exit(EXIT_FAILURE);
                run_full_align(0, sReferenceFile, sReadsFile, sOutputFile);
            }

            /*
            cout << "running getReads()" << endl;
            if (getReads((sUnalignedFile + "_1"), (sAlignedFile), "-f 4", "", true) != 0)
                cout << "getReads 1 exited incorrectly" << endl;
            */
        }

        if (confDB.getKey("extractUnalignedReads").boolVal == true){

            cout << "running convertSAMtoFASTA()" << endl;
            if (NUM_THREADS > 1) {
                cout << "Making threads" << endl;
                std::vector<std::thread> t;
                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    std::string str_i = std::to_string(i);
                    //t.push_back(thread(run_get_reads, i, (sUnalignedFile + "_1"), (sAlignedFile + str_i), "-f 4", "", true));
                    t.push_back(thread(run_convertSAMtoFASTA, i, (sUnalignedFile + str_i + "_1")));
                } 

                // join threads here
                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    t[i].join();
                } 
                cout << "Threads rejoined" << endl;

            } else {
                exit(EXIT_FAILURE);
                run_full_align(0, sReferenceFile, sReadsFile, sOutputFile);
            }

            /*
            cout << "running convertSAMtoFASTA()" << endl;
            // Next we take the unaligned reads SAM file and create a FASTA file
            if (convertSAMtoFASTA(sUnalignedFile + "_1") != 0)
                cout << "convertSAMtoFASTA 1 exited incorrectly" << endl;
            */
        }

        if (confDB.getKey("halfAlign").boolVal == true){

            cout << "running executeBwaAligner()" << endl;
            if (NUM_THREADS > 1) {
                cout << "Making threads" << endl;
                std::vector<std::thread> t;
                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    std::string str_i = std::to_string(i);
                    //t.push_back(thread(run_get_reads, i, (sUnalignedFile + "_1"), (sAlignedFile + str_i), "-f 4", "", true));
                    t.push_back(thread(run_executeBwaAligner, i, sReferenceFile,(sUnalignedFile + str_i + "_1.fasta"), (sOutputFile + str_i + "_2")));

                } 

                // join threads here
                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    t[i].join();
                } 
                cout << "Threads rejoined" << endl;

            } else {
                exit(EXIT_FAILURE);
            }

            /*
            cout << "running executeBwaAligner()" << endl;
            // Now rerun the BWA aligner with the new unaligned reads file
            if (executeBwaAligner(sReferenceFile,(sUnalignedFile + "_1.fasta"), (sOutputFile + "_2")) != 0)
            cout << "BWA aligner 2 exited incorrectly" << endl;
            */
        }

        if (confDB.getKey("extractHalfReads").boolVal == true){
            // Then we get unaligned reads (flag of 0x4, 0x16)
            //[>if (getReads((sUnalignedFile + "_2"), (sOutputFile + "_2.sam"), "-f 4", "-f 16", false) != 0)
                //cout << "getReads 2 exited incorrectly" << endl;

            cout << "running getReads()" << endl;
            if (NUM_THREADS > 1) {
                cout << "Making threads" << endl;
                std::vector<std::thread> t;
                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    std::string str_i = std::to_string(i);
                    t.push_back(thread(run_get_reads, i, (sUnalignedFile + str_i + "_2"), (sOutputFile + str_i + "_2.sam"), "-f 4", "", false));

                } 

                // join threads here
                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    t[i].join();
                } 
                cout << "Threads rejoined" << endl;

            } else {
                exit(EXIT_FAILURE);
            }

            /*
            cout << "running getReads()" << endl;
            if (getReads((sUnalignedFile + "_2"), (sOutputFile + "_2.sam"), "-f 4", "", false) != 0)
                cout << "getReads 2 exited incorrectly" << endl;
            */
        }

        if (confDB.getKey("filterOut").boolVal == true){
            // Finally lets take the new BWA SAM file and use samtools to find the matches (flag 0x0)
            //[>if (filterOut(("bwaAligned"), (sOutputFile + "_2"), "-F 4", "-F 16") != 0)
                cout << "getReads 3 exited incorrectly" << endl;


            cout << "running filterOut()" << endl;
            if (NUM_THREADS > 1) {
                cout << "Making threads" << endl;
                std::vector<std::thread> t;
                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    std::string str_i = std::to_string(i);
                    t.push_back(thread(run_filterOut, i, (sFinalAlignedFile + str_i), (sOutputFile + str_i + "_2"), "-F 4", ""));

                } 

                // join threads here
                for (unsigned int i = 0; i < NUM_THREADS; ++i) {
                    t[i].join();
                } 
                cout << "Threads rejoined" << endl;

            } else {
                exit(EXIT_FAILURE);
            }

            /*
            cout << "running filterOut()" << endl;
            if (filterOut((sFinalAlignedFile), (sOutputFile + "_2"), "-F 4", "") != 0)
                cout << "getReads 3 exited incorrectly" << endl;
            */
        }

    }

    cout << "\nBWA time = " << (int)time(NULL)-time0 << endl;
    fLogFileOut << "\nBWA time = " << (int)time(NULL)-time0 << endl;

    std::cout << "startExecutables finished" << std::endl; 
    std::cout << "Concatenating temp files" << std::endl;


    // bwaOutput*_1.sam
    std::vector<std::string> bwaOutput_1_input_v;
    std::vector<std::string> bwaOutput_2_input_v;
    std::vector<std::string> unaligned_1_sam_input_v;
    std::vector<std::string> unaligned_2_sam_input_v;
    std::vector<std::string> unaligned_1_fasta_input_v;
    std::vector<std::string> bwaAligned_input_v;

    for (size_t i = 0; i < NUM_THREADS; ++i) {
        bwaOutput_1_input_v.push_back(sProjectDirectory + sOutputFile + std::to_string(i) + "_1.sam");
        bwaOutput_2_input_v.push_back(sProjectDirectory + sOutputFile + std::to_string(i) + "_2.sam");
        unaligned_1_sam_input_v.push_back(sProjectDirectory + sUnalignedFile + std::to_string(i) + "_1.sam");
        unaligned_1_fasta_input_v.push_back(sProjectDirectory + sUnalignedFile + std::to_string(i) + "_1.fasta");
        unaligned_2_sam_input_v.push_back(sProjectDirectory + sUnalignedFile + std::to_string(i) + "_2.sam");
        bwaAligned_input_v.push_back(sProjectDirectory + "bwaAligned" + std::to_string(i) + ".sam");
    }
    //concatenate_to_file(bwaOutput_1_input_v, sProjectDirectory + sOutputFile + "_1.sam");
    concatenate_to_file(bwaOutput_2_input_v, sProjectDirectory + sOutputFile + "_2.sam"); 
    concatenate_to_file(unaligned_1_sam_input_v, sProjectDirectory + sUnalignedFile + "_1.sam"); 
    concatenate_to_file(unaligned_1_fasta_input_v, sProjectDirectory + sUnalignedFile + "_1.fasta");
    concatenate_to_file(unaligned_2_sam_input_v, sProjectDirectory + sUnalignedFile + "_2.sam");
    concatenate_to_file(bwaAligned_input_v, sProjectDirectory + "bwaAligned.sam");

    if (confDB.getKey("onlyAlign").boolVal == true)
        exit(1);

    //exit(EXIT_SUCCESS);
    return 0;
}

int executeBwaIndex(string sReferenceFile){

    cout << "; running executeBwaIndex()" << endl;

    string command;

    // start BWA index
    cout << "\nstarting bwa index..." << endl;
    fLogFileOut << "\nstarting bwa index..." << endl;
    command = "./bwa index -a bwtsw " + sReferenceFile;
    cout << "running command: " << command << endl;
    if (system(command.c_str()) != 0) {
        cout << "error: ensure bwa is in the same directory as " << PROGRAM_NAME << endl;
        std::exit(EXIT_FAILURE);
    }
    cout << "bwa index finished..." << endl;
    fLogFileOut << "bwa index finished..." << endl;

    return 0;
}

int executeBwaAligner(string sReferenceFile, string sReadsFile, string sOutputFile){
    cout << "; running executeBwaAligner()" << endl;
    string command;

    cout << "sReferenceFile " << sReferenceFile << ", sReadsFile " << sReadsFile << ", sOutputFile " << sOutputFile << endl;
    // run the BWA alignment algorithm
    cout << "\nstarting bwa aligner..." << endl;
    fLogFileOut << "\nstarting bwa aligner..." << endl;
    command = "./bwa aln -n 4 " + sReferenceFile + " " + sProjectDirectory + sReadsFile + " | ./bwa samse " + sReferenceFile + " - " + sProjectDirectory + sReadsFile + " > " + sProjectDirectory + sOutputFile + ".sam";
    cout << "command: " << command << endl;
    system(command.c_str());
    cout << "bwa aligner finished..." << endl;
    fLogFileOut << "bwa aligner finished..." << endl;

    return 0;
}

int getReads(string sUnalignedFile, string sBwaOutputFile, string sFlag1, string sFlag2, bool bFlagFirst){

    cout << "; running getReads()" << endl;

    cout << "sUnalignedFile: " << sUnalignedFile << ", sBwaOutputFile: " 
         << sBwaOutputFile << ", sFlag1: " << sFlag1 << ", sFlag2: " 
         << ", bFlagFirst: " <<  bFlagFirst << endl;

    bool bPairedEnd = confDB.getKey("pairedEnd").boolVal;
    string command;
    cout << "Running getReads() using " << sProjectDirectory << sBwaOutputFile << endl;

    if (bPairedEnd){
        cout << "\n** Paired-end functionality has not been implemented yet" << endl;
        exit(1);
    } else {
        if (confDB.getKey("bamFile").boolVal == true && bFlagFirst == true)
            command = "samtools view -h " + sFlag1 + " " + 
                sProjectDirectory + sBwaOutputFile + " > " + 
                sProjectDirectory + sUnalignedFile + "_flag_1.sam";
        else
            command = "samtools view -h " + sFlag1 + " -S " + 
                sProjectDirectory + sBwaOutputFile + " > " + 
                sProjectDirectory + sUnalignedFile + "_flag_1.sam";

        // run samtools to get unaligned reads
        cout << "\nget samtools reads with flag " + sFlag1 + " into " << sProjectDirectory + sUnalignedFile << "_flag_1..." << endl;
        fLogFileOut << "\nget samtools reads with flag " + sFlag1 + " into " << sProjectDirectory + sUnalignedFile << "_flag_1..." << endl;

        std::cout << "Running: " << command << std::endl;
        system(command.c_str());

        if (sFlag2.compare("") != 0){
            command = "samtools view -h " + sFlag2 + " -S " + 
                sProjectDirectory + sBwaOutputFile + " > " + sProjectDirectory + sUnalignedFile + "_flag_2.sam";

            cout << "\nget samtools reads with flag " + sFlag2 + " into " << sProjectDirectory + sUnalignedFile << "_flag_2..." << endl;
            fLogFileOut << "get samtools reads with flag " + sFlag2 + " into " << sProjectDirectory + sUnalignedFile << "_flag_2..." << endl;
            system(command.c_str());

            // merge two files together
            cout << "\nmerging SAM files..." << endl;
            fLogFileOut << "merging SAM files..." << endl;
            command = "cat " + sProjectDirectory + sUnalignedFile + "_flag_1.sam > " + sProjectDirectory + sUnalignedFile + ".sam";
            system(command.c_str());

            command = "cat " + sProjectDirectory + sUnalignedFile + 
                "_flag_2.sam | awk '$1 !~ /@/' >> " + sProjectDirectory + sUnalignedFile + ".sam";
            system(command.c_str());

            // remove _flag files
            cout << "\nremoving _flag files..." << endl;
            fLogFileOut << "removing _flag files..." << endl;
            command = "rm *flag_*.sam";
            system(command.c_str());
        } else {
            // move _flag_1 to regular .sam file
            command = "mv " + sProjectDirectory + sUnalignedFile + "_flag_1.sam " + sProjectDirectory + sUnalignedFile + ".sam";
            system(command.c_str());
        }
    }

    return 0;
}

int convertSAMtoFASTA(string sUnalignedFile){
    cout << "sUnalignedFile: " << sUnalignedFile << endl;
    cout << "; running convertSAMtoFASTA()" << endl;
    string command = "";
    command = "samtools view -S " + sProjectDirectory + sUnalignedFile + ".sam | ";
    command += " awk '{OFS=\"\t\"; print \">\"$1\"-1\\n\"substr($10,1,length($10)/2)\"\\n>\"$1\"-2\\n\"substr($10,length($10)/2+1,length($10))}' - > ";
    command += sProjectDirectory + sUnalignedFile + ".temp";

    cout << "\nConverting SAM to FASTA..." << endl;
    fLogFileOut << "\nConverting SAM to FASTA..." << endl;
    system(command.c_str());

    ifstream input;
    ofstream output;
    string line = sProjectDirectory + sUnalignedFile + ".temp";
    input.open(line.c_str());
    line = sProjectDirectory + sUnalignedFile + ".fasta";
    output.open(line.c_str());
    //int iLength = 0; // length of the nucleotide sequence
    unsigned int iMinSeqLength = confDB.getKey("minSeqLength").intVal;
    int count = 1;
    int iTooShort = 0;
    string header = "";

    while (input.good()){
        getline(input, line);

        if (line[0] == '>'){
            header = line;
            ++count;
            continue;
        }
        if (line.length() < iMinSeqLength && count == 2){
            getline(input, line);
            getline(input, line);
            count = 1;
            ++iTooShort;
            continue;
        }
        output << header << endl;
        output << line << endl;
        ++count;
        if (count == 5)
            count = 1;
    }
    cout << "filtered reads too short: " << iTooShort << endl;
    fLogFileOut << "filtered reads too short: " << iTooShort << endl;
    cout << "finished..." << endl;
    input.close();
    output.close();

    command = "rm " + sProjectDirectory + sUnalignedFile + ".temp";
    system(command.c_str());
    return 0;
}

int filterOut (string sOutputFile, string sInputFile, string sFlag1, string sFlag2){
    string command;
    cout << "\nfiltering " + sFlag1 + " out of " + sProjectDirectory + sInputFile + " into " + sProjectDirectory + sOutputFile + ".temp..." << endl;
    fLogFileOut << "\nfiltering " + sFlag1 + " out of " + sProjectDirectory + sInputFile + " into " + sProjectDirectory + sOutputFile + ".temp..." << endl;
    command = "samtools view -h " + sFlag1 + " -S " + sProjectDirectory + sInputFile + ".sam > " + sProjectDirectory + sOutputFile + ".temp";
    system(command.c_str());

    if (sFlag2.compare("") != 0){
        cout << "filtering " + sFlag2 + " out of " + sProjectDirectory + sInputFile + " into " + sProjectDirectory + sOutputFile + ".sam..." << endl;
        fLogFileOut << "filtering " + sFlag2 + " out of " + sProjectDirectory + sInputFile + " into " +sProjectDirectory +  sOutputFile + ".sam..." << endl;
        command = "samtools view -h " + sFlag2 + " -S " + sProjectDirectory + sOutputFile + ".temp > " + sProjectDirectory + sOutputFile + ".sam";
        system(command.c_str());

        cout << "\ncleaning up..." << endl;
        fLogFileOut << "cleaning up..." << endl;
        command = "rm " + sProjectDirectory + sOutputFile + ".temp";
        system(command.c_str());
    } else {
        // move _temp to regular .sam file
        command = "mv " + sProjectDirectory + sOutputFile + ".temp " + sProjectDirectory + sOutputFile + ".sam";
        system(command.c_str());
    }
    return 0;
}
