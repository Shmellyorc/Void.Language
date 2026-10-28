# Approved generic operator access amendment

Boss / Golden Wrench approval: 28 October 2026, during collections engineering.
The previous Stage 10 rule was intentional; this is an explicitly authorized
language extension, not a claim that the prior implementation was defective.

In a concrete generic specialization, a public operator on the exact public
nominal owner of a substituted type may be selected without a direct import of
that owner in the generic definition's module. Inferred and explicit type
arguments behave identically. A substituted list of nodes does not grant access
to node's operators: the exact substituted nominal type must itself be an owner.
The ordinary selector still requires exact operands, kind/nullability, glyph and
arity and still diagnoses ambiguity. It performs no conversion, ancestor search,
transitive import inheritance or caller-scope lookup. Readonly/effect rules and
ordinary call evaluation remain unchanged. Private/internal operators and
nonpublic nominal owners receive no additional access. Ordinary non-generic code
retains the original direct-import restriction.

The typed IR carries non-executable generic declaration arities and typed concrete
instance records, separately from executable functions and nominal types. The
independent validator verifies declaration scope, argument validity/arity, exact
length-prefixed canonical specialization identity and the corresponding concrete
function or nominal owner. The selector rederives the eligible owners from those
validated type arguments. There is no allow-access flag or unchecked callee string.
Generic methods derive their concrete declaring identity from the exact receiver
and, when that receiver is generic, its separately verified nominal declaration
and arguments. Construction variants derive their suffix from their formal
construction parameters. Neither form carries an extra access grant.
Generic owner methods use their concrete nominal instance; nested source closures
must prove their lexical creation and parent chain before inheriting a context.
Removing a proof, substituting another argument, inventing a declaration or
changing the declared arity or forging the bound receiver rejects. Entry-graph emission prunes unused function
instance proofs with the functions. Runtime dispatch and native ABI are unchanged.

`tests/stage10/operator_resolution.cpp` updates the historical generic negative
under this approval while retaining ordinary transitive-import/private-owner
negatives. It adds inferred/explicit positives, private/internal and unrelated
owner negatives and malformed IR provenance cases. Collections tests exercise
custom equality effects in index_of/contains, including same-count structural
mutation, immediate return, a new iterator and an already-active outer iterator.
Final profile/tooling/cache/reload evidence remains part of the collections
acceptance matrix; this amendment is not an independent release lock.
