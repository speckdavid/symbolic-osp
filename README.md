# Symbolic Osp Planner Sym-Osp

Sym-Osp is a state-of-the-art optimal oversubscription planner based on symbolic search.

Main source:
 - Speck, D.; Michael Katz. 2021. Symbolic Search for Oversubscription Planning. In Proceedings of the Thirty-Fifth AAAI Conference on Artificial Intelligence (AAAI 2021), AAAI Press. (to appear)

```console
@InProceedings{speck-et-al-aaai2021,
  author =       "David Speck and Michael Katz",
  title =        "Symbolic Search for Oversubscription Planning",
  booktitle =    "Proceedings of the Thirty-Fifth {AAAI} Conference on
                  Artificial Intelligence ({AAAI} 2021)",
  publisher =    "{AAAI} Press",
  year =         "2021",
  note =         "to appear"
}
```

## Dependencies
Currently we only support Linux systems. The following should install all necessary dependencies.
```console
$ sudo apt-get -y install cmake g++ make python3 autoconf automake
```

Sym-Osp should compile on MacOS with the GNU C++ compiler and clang with the same instructions described above.
 
## Compiling the Symbolic Osp Planner

```console
$ ./build.py 
```

## Osp Configurations

Sym-Osp uADD representing the utility function as ADD to determine the utility values of sets of states.
```console
$ ./fast-downward.py --translate --search domain.pddl problem.pddl --search "symosp-fw()"
```

Sym-Osp uBDD representing the utility function as multiple BDDs to determine the utility values of sets of states.
```console
$ ./fast-downward.py --translate --search domain.pddl problem.pddl --search "symosp-fw(use_add=false)"
```

Explicit A\* search with the blind heuristic, representing the utility function as an ADD to determine the utility values of a single state.
```console
$ ./fast-downward.py --translate --search domain.pddl problem.pddl --search "eager_osp(single(g()), f_eval=g(),reopen_closed=true)"
```

## Based on:
 - Fast Downward: http://www.fast-downward.org/
 - Symbolic Fast Downward: https://fai.cs.uni-saarland.de/torralba/software.html
 - Symbolic Top-k: https://github.com/speckdavid/symk
