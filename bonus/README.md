# Bonus: Generational Tracing GC (Future Scope)

## Status

Not implemented in the current submission.

## Intent

Move from manual lifetime management toward automatic reclamation for long-running multi-task systems.

## Planned Direction

- Add mark-and-sweep reachability traversal.
- Introduce young/old generations for cheaper common-case collection.
- Define root discovery strategy for stack/globals/task-owned references.
- Integrate safely with existing handle-based allocator model.
