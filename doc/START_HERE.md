# foster-btree internal documentation #

The `doc` folder contains documentation of the foster-btree design and architecture, including the organization of files, modules, and classes in the source code.

Using the provided `Doxyfile`, a more reader-friendly, HTML-based documentation of the source code can be generated using the program `doxygen`. This documentation is generated from structured comments in the code, so feel free to read through the source files directly if you would like to skip this step.

The present file was created to provide a higher-level overview of our implementation. Therefore, we recomend reading it before diving into the source code or the doxygen documentation.

## Academic background ##

This implementation is based on the following publications:

* Goetz Graefe, Hideaki Kimura, Harumi A. Kuno: [Foster b-trees](http://doi.acm.org/10.1145/2338626.2338630). ACM Trans. Database Syst. (TODS) 37(3):17 (2012)
* Goetz Graefe: [Modern B-Tree Techniques](http://www.nowpublishers.com/article/Details/DBS-028). Foundations and Trends in Databases 3(4): 203-402 (2011)

The first paper introduced the Foster B-tree data structure, which itself is a combination of previous B-tree designs -- all of which are well documented in the bibliography section. The second paper discusses implementation techniques for efficient data management with B-trees. Despite the publication predating the introduction of Foster B-trees, these techniques apply to all B-trees in general.

Please note that our `foster-btree` library does not support transactions -- i.e., it does not implement locking, rollback, logging, or recovery. It is designed to be a low-level B-tree data structure upon which transactional capabilities can be implemented in an extensible manner. We are working on such functionality in the following repository:

https://github.com/caetanosauer/fineline

## Metaprogramming and generic classes ##

TODO

## Architecture ##

TODO

## Next steps ##

TODO
