//============================================================================
// Name        : test_AEStblSpeed.cpp
// Author      : Dusan Klinec (ph4r05)
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
// *  Author: Dusan Klinec (ph4r05)
// *
// *  License: GPLv3 [http://www.gnu.org/licenses/gpl-3.0.html]
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <vector>
#include <iostream>
#include <fstream>

#define WBAES_BOOTS_SERIALIZATION 1

// NTL dependencies
#include <NTL/GF2.h>
#include <NTL/GF2X.h>
#include <NTL/vec_GF2.h>
#include <NTL/GF2E.h>
#include <NTL/GF2EX.h>
#include <NTL/mat_GF2.h>
#include <NTL/vec_long.h>

#include "GenericAES.h"
#include "NTLUtils.h"
#include "MixingBijections.h"
#include "WBAES.h"
#include "WBAESGenerator.h"
#include "BGEAttack.h"
NTL_CLIENT

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
namespace po = boost::program_options;

using namespace std;
using namespace NTL;
using namespace boost;
using namespace wbacr;
using namespace wbacr::laeqv;
using namespace wbacr::attack;

int main(int argc, const char * argv[]) {
	time_t start=0, end=0;
	bool useExternal = false;
	int benchgen=0;
	int benchbge=0;
	std::string outFile = "";

	// very poor PRNG seeding, but just for now
	srand((unsigned)time(0));
	GF2X defaultModulus = GF2XFromLong(0x11B, 9);
	GF2E::init(defaultModulus);

	// input parameters processing
	po::options_description description("WBAES table implementation usage");
	description.add_options()
		("help,h",                                                                         "Display this help message")
		("bench-gen",      po::value<int>()->default_value(0)->implicit_value(0),          "Benchmarking rounds for AES gen")
		("bench-bge",      po::value<int>()->default_value(0)->implicit_value(0),          "Benchmarking rounds for AES BGE attack")
		("extEnc,e",       po::value<bool>()->default_value(false)->implicit_value(false), "Use external encoding?")
		("out-file,o",     po::value<std::string>(),                                       "Output file to write encrypted data")
		("input-files",    po::value<std::vector<std::string>>(),                          "Input files")
		("create-table",   po::value<std::string>(),                                       "Create encryption/decryption tables")
		("version,v",                                                                      "Display the version number");


    po::positional_options_description p;
    p.add("input-files", -1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(description).positional(p).run(), vm);
    po::notify(vm);

    if(vm.count("help")){
        std::cout << description;

        return 0;
    }

    if(vm.count("version")){
        std::cout << "WBAES table implementation version 0.1" << std::endl;
        return 0;
    }

    if (vm.count("out-file")){
    	outFile = vm["out-file"].as<std::string>();
    	std::cout << "Output file given: " << outFile << endl;
    }

    // use external coding ?
    useExternal = vm["extEnc"].as<bool>();

    //
    // AES generator benchmark
    //
    benchgen = vm["bench-gen"].as<int>();
    if (benchgen > 0){
    	cout << "Benchmark of the WB-AES generator is starting..." << endl;

    	//
		// Encryption with WB AES time test
		//
		GenericAES defAES;
		defAES.init(0x11B, 0x03);

		WBAESGenerator generator;

		cout << "External coding will be used: " << useExternal << endl;
		ExtEncoding coding;
		generator.generateExtEncoding(&coding, useExternal ? 0 : WBAESGEN_EXTGEN_ID);

		cout << "Going to compute tables. Benchmark will iterate " << benchgen << " times" << endl;
		time(&start);

		WBAES * genAES = new WBAES;
		for(int i=0; i<benchgen; i++){
			generator.generateTables(GenericAES::testVect128_key, KEY_SIZE_16, genAES, &coding, true);
			generator.generateTables(GenericAES::testVect128_key, KEY_SIZE_16, genAES, &coding, false);

			time(&end);
			cout << "Done i="<<i<<"; time elapsed=" << (end-start) << endl;
		}

		delete genAES;
		time_t total = end-start;

		cout << "Benchmark finished! Total time = " << (total) << " s; on average = " << (total / (float)benchgen) << " s" << endl;
    }

    //
	//BGE benchmark
	//
    benchbge = vm["bench-bge"].as<int>();
    if (benchbge > 0){
    	cout << "Benchmark of the BGE attack is starting..." << endl;

    	//
		// Encryption with WB AES time test
		//
    	time_t total = 0;
		GenericAES defAES;
		defAES.init(0x11B, 0x03);

		WBAESGenerator generator;
		ExtEncoding coding;
		generator.generateExtEncoding(&coding, WBAESGEN_EXTGEN_ID);

		cout << "Going to generate AES tables to crack ..." << endl;

		WBAES     * genAES = new WBAES;
		WBAES     * tmpAES = new WBAES;
		BGEAttack * atk = new BGEAttack;

		clock_t pstart, pend;
		clock_t pacc = 0;

		generator.generateTables(GenericAES::testVect128_key, KEY_SIZE_16, genAES, &coding, true);
		generator.generateTables(GenericAES::testVect128_key, KEY_SIZE_16, genAES, &coding, false);
		cout << "Tables generated, starting with the attack; size of the AES struct = " << sizeof(WBAES) << endl;
		for(int i = 0; i < benchbge; i++){
			memcpy(tmpAES, genAES, sizeof(WBAES));
			atk->wbaes = tmpAES;

			cout << "Starting round="<<i<<endl;

			time(&start);
			pstart = clock();

			atk->attack();

			pend = clock();
			time(&end);

			total += end-start;
			pacc  += pend - pstart;
		}

		delete genAES;
		delete atk;
		delete tmpAES;
		cout << "Benchmark finished! Total time = " << (total) << " s; on average = " << (total / (float)benchbge) << " s"
				<< "; clocktime=" << ((float) pacc / CLOCKS_PER_SEC) << " s;" << endl;
    }


    //
    // AES encryption - encrypt input files with table representation
    //
	if(vm.count("input-files")){
		std::vector<std::string>  files = vm["input-files"].as<std::vector<std::string>>();
		for(std::string file : files){
			std::cout << "Input file " << file << std::endl;
		}

		//
		// Encryption with WB AES time test
		//
		GenericAES defAES;
		defAES.init(0x11B, 0x03);

		WBAESGenerator generator;
		WBAES * genAES = new WBAES;

		cout << "External coding will be used: " << useExternal << endl;
		ExtEncoding coding;
		generator.generateExtEncoding(&coding, useExternal ? 0 : WBAESGEN_EXTGEN_ID);

		cout << "Generating WB-AES instance..." << endl;
		time(&start);
		generator.generateTables(GenericAES::testVect128_key, KEY_SIZE_16, genAES, &coding, true);
		generator.generateTables(GenericAES::testVect128_key, KEY_SIZE_16, genAES, &coding, false);
		time(&end);
		cout << "Generating AES tables took: ["<<(end-start)<<"] seconds" << endl;

		// open the given file
		std::string fileName = files[0];
		cout << "Going to encrypt file ["<<fileName<<"] with WBAES" << endl;

		bool writeOut = !outFile.empty();
		ofstream out;
		if (writeOut){
			out.open(outFile.c_str(), ios::out | ios::binary | ios::trunc);
		}

		// Open reading file
		ifstream inf(fileName.c_str(), ios::in | ios::binary | ios::ate);
		if (inf.is_open()==false){
			cerr << "Cannot open specified input file" << endl;
			exit(3);
		}

		// read the file
		const int buffSize       = 4096;
		const long int iters     = buffSize / N_BYTES;
		unsigned long long blockCount = 0;
		char * memblock          = new char[buffSize];
		char blockbuff[N_BYTES];

		ifstream::pos_type size;
		// get file size - ios::ate => we are at the end of the file
		size = inf.tellg();
		// move on the beginning
		inf.seekg(0, ios::beg);

		// time measurement of just the cipher operation
		time_t cstart, cend;
		time_t cacc=0;

		clock_t pstart, pend;
		clock_t pacc = 0;

		// measure the time here
		time(&start);
		do {
			streamsize bRead;

			// read data from the file to the buffer
			inf.read(memblock, buffSize);
			bRead = inf.gcount();
			if (inf.bad()) {
				std::cout << "badBit. Bytes read:" << bRead << " could be read";
				break;
			}

			// here we have data in the buffer - lets encrypt them
			W128b plain, state;
			long int iter2comp = max(iters, (long int) ceil((float)bRead / N_BYTES));

			for(int k = 0; k < iter2comp; k++, blockCount++){
				arr_to_W128b(memblock, N_BYTES * 16, plain);

				// encryption
				if (useExternal) generator.applyExternalEnc(state, &coding, true);

				time(&cstart);
				pstart = clock();
				genAES->encrypt(state);
				pend = clock();
				time(&cend);

				cacc += (cend - cstart);
				pacc += (pend - pstart);

				if (useExternal) generator.applyExternalEnc(state, &coding, false);

				// if wanted, store to file
				if (writeOut){
					W128b_to_arr(blockbuff, 0, state);
					out.write(blockbuff, N_BYTES);
				}

				if ((blockCount % 320) == 0){

				}
			}

			if (inf.eof()){
				cout << "Finished reading the file " << endl;
				break;
			}
		} while(true);
		time(&end);

		time_t total = end-start;
		cout << "Encryption ended in ["<<total<<"]s; Pure encryption took ["<<((float) pacc / CLOCKS_PER_SEC)
					<<"] s (clock call); time: ["<<cacc<<"] s; encrypted ["<<blockCount<<"] blocks" << endl;

		// free allocated memory
		delete genAES;
		delete[] memblock;
		// close reading file
		inf.close();
		// close output writing file
		if (writeOut){
			out.flush();
			out.close();
		}
	}
}

int A1A2relationsGenerator(void){
	// very poor PRNG seeding, but just for now
	srand((unsigned)time(0));
	GF2X defaultModulus = GF2XFromLong(0x11B, 9);
	GF2E::init(defaultModulus);

	ofstream dump;
	ofstream dumpA;
	dump.open("/media/share/AES_A1A2dump.txt");
	dumpA.open("/media/share/AES_signature.txt");


	dump  << "polynomial;generator;qq;ii;problems;A1;A2" << endl;
	dumpA << "polynomial;generator;sbox;sboxinv;mixcol;mixcolinv" << endl;

	GenericAES defAES;
	defAES.init(0x11B, 0x03);
	defAES.printAll();

	int AES_gen, AES_poly;
	for(AES_poly=0; AES_poly < AES_IRRED_POLYNOMIALS; AES_poly++){
		for(AES_gen=0; AES_gen < AES_GENERATORS; AES_gen++){
			GenericAES dualAES;
			dualAES.initFromIndex(AES_poly, AES_gen);

			// write to file
			dumpA   << CHEX(GenericAES::irreduciblePolynomials[AES_poly]) << ";"
					<< CHEX(GenericAES::generators[AES_poly][AES_gen]) << ";";
			dumpVector(dumpA, dualAES.sboxAffineGF2E, 256); dumpA << ";";
			dumpVector(dumpA, dualAES.sboxAffineInvGF2E, 256); dumpA << ";";
			dumpMatrix(dumpA, dualAES.mixColMat); dumpA << ";";
			dumpMatrix(dumpA, dualAES.mixColInvMat); dumpA << ";";
			dumpA << endl;
			dumpA.flush();

			cout << "+";
			int ii=0,qq=0,probAll=0;
			for(qq=0;qq<8; qq++){
				cout << ".";
				for(ii=1;ii<256;ii++){
					int problems=0;
					vec_GF2E A1;
					vec_GF2E A2;
					dualAES.generateA1A2Relations(A1, A2, ii, qq);
					problems = dualAES.testA1A2Relations(A1, A2);

					// write to file
					dump    << CHEX(GenericAES::irreduciblePolynomials[AES_poly]) << ";"
							<< CHEX(GenericAES::generators[AES_poly][AES_gen]) << ";"
							<< qq << ";" << ii << ";" << problems << ";";
					dumpVector(dump, A1);
					dump << ";";
					dumpVector(dump, A2);
					dump << endl;

					if (problems>0){
						cout << "Current Dual AES: "
								<< CHEX(GenericAES::irreduciblePolynomials[AES_poly]) << ";"
								<< CHEX(GenericAES::generators[AES_poly][AES_gen]) << endl;
						cout << "Problem with relations ii="<<ii<<"; qq="<<qq<<"; problems=" << problems << endl;
						probAll+=1;
					}
				}
			}

			// force write
			dump.flush();
		}
	}

	dumpA.close();
	dump.close();
	return 0;
}

int dualAESTest(void){
	// very poor PRNG seeding, but just for now
	srand((unsigned)time(0));
	GF2X defaultModulus = GF2XFromLong(0x11B, 9);
	GF2E::init(defaultModulus);

	GenericAES defAES;
	defAES.init(0x11B, 0x03);
	defAES.printAll();
	defAES.testWithVectors();

	GenericAES dualAES;
	dualAES.init(0x11D, 0x9d);
	//dualAES.initFromIndex(15,5);
	dualAES.printAll();
	dualAES.testWithVectors();

	// try round key expansion
	vec_GF2E roundKey;
	vec_GF2E key;
	key.SetLength(128);
	dualAES.expandKey(roundKey, key, KEY_SIZE_16);
	cout << "Round key for ZERO key for 16B: " << endl;
	dumpVector(roundKey);

	mat_GF2E state(INIT_SIZE, 4, 4);
	cout << "Plaintext: " << endl;
	dumpMatrix(state);

	cout << "Testing encryption: " << endl;
	dualAES.encryptInternal(state, roundKey);
	dumpMatrix(state);

	dualAES.applyTinv(state);
	cout << "Testing encryption AFTER Tinv: " << endl;
	dumpMatrix(state);

	dualAES.applyT(state);
	cout << "Testing backward decryption: " << endl;
	dualAES.decryptInternal(state, roundKey);
	dumpMatrix(state);

	cout << "Multiplication matrix: " << endl;
	mat_GF2 multA= dualAES.makeMultAMatrix(2);
	dumpMatrix(multA);

	cout << "Squaring matrix: " << endl;
	mat_GF2 sqr = dualAES.makeSquareMatrix(1);
	dumpMatrix(sqr);


	cout << "A1 and A2 relations, testing all possible" << endl;
	int ii=0,qq=0,probAll=0;
	for(qq=0;qq<8; qq++){
		for(ii=1;ii<256;ii++){
			int problems=0;
			vec_GF2E A1;
			vec_GF2E A2;
			dualAES.generateA1A2Relations(A1, A2, ii, qq);
			problems = dualAES.testA1A2Relations(A1, A2);

			if (problems>0){
				cout << "Problem with relations ii="<<ii<<"; qq="<<qq<<"; problems=" << problems << endl;
				probAll+=1;
			}
		}
	}

	cout << "All relations tested, problemsAll = " << probAll << endl;

	vec_GF2E A1;
	vec_GF2E A2;
	dualAES.generateA1A2Relations(A1, A2, 1+(rand() % 0xfe), rand() % 7);
	cout << "Testing relations A1 A2: Problems = " << dualAES.testA1A2Relations(A1, A2) << endl;

	cout << "A1: " << endl;
	dumpVector(A1);

	cout << "A2: " << endl;
	dumpVector(A2);

	cout << "Generating random bijections: " << endl;
	vec_GF2X rndB;
	vec_GF2X rndBinv;
	generateRandomBijection(rndB, rndBinv, AES_FIELD_SIZE, AES_FIELD_DIM);
	dumpVector(rndB);
	dumpVector(rndBinv);

	return 0;
}



