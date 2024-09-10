# Hacking NapSAT

## Folder structure

```ini {"id":"01J7E2P87QGT7TJM70BYMFND2R"}
├── include
├── src
│   ├── solver
│   ├── observer
│   ├── display
│   ├── proof
│   └── utils
└── tests
│   └── cnf
├── scripts
└── invariant-configurations
```

- `include`: contains the library headers. A user willing to use the library should only require the headers in this directory.
- `src`: contains the source code of the library. The code is divided into different modules:

   - `solver`: core of the solver is located in this folder. That is, the implementation of the CDCL algorithm.
   - `observer`: module that allows the SAT solver to send notifications such that its state can be observed. The observer contains a simplified version of the solver's state that allows to
      - Display the state of the solver
      - Check invariants
      - Compute statistics
      - Generate LaTeX diagrams (for slides)
      - Navigate the execution (back and forth)
      - Debug the solver
      - ...

   - `display`: responsible for displaying the content of the observer. It is also used to interact with the solver during its execution.
   - `proof`: This module is responsible for generating proofs of unsatisfiability. The proof is generated in the form of a resolution tree.
   - `utils`: contains utility functions that are used by the other modules.

- `tests`: contains the tests of the library.

   - `cnf`: contains some files in the DIMACS CNF format that are used for testing the solver. It also contains commands to interact with the solver and force specific executions. For example, the `test-trigger-mli.cnf` file contains the example used in the SAT24 paper on lazy reimplication in chronological backtracking.

- `scripts`: a few scripts for the quality of life of the developers.
   For example, `generate-option-documentation.py` generates the help message for the command line based on the `include/SAT-options.hpp`.
- `invariant-configurations`: contains the different invariants that are checked by the observer module. Different backtracking strategies ensure different invariants.

## Testing Framework

When developing new features, it is recomended to write tests. We use the Catch2 testing framework.
All tests are located in the `tests` folder.

### install dependencies

To install the dependencies, run the following command:

```sh {"id":"01J47DMF382YJYWQZAK8NA9D98"}
make install-test
```

The tests can be compiled using the following command:

```sh {"id":"01J47DPH1MD6YZYC3GKR6HXDNA"}
make tests
```

Finally, to run the tests, first go to the build directory, and then run the executable:

```sh {"id":"01J47DQDSR070GH89D4WY3371M"}
cd build
./NapSAT-tests
```