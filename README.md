# Teczka

## Equities List
Each portfolio will contain a list of equities. An equity is uniquely identified by a key.
In this application, I only care about stocks, so an each equity will have a stock ticker
as its key. This list will be a shared data structure (used concurrently by multiple
threads). Because of this, we will need to implement some mechanism to prevent race
conditions. This section will outline the design decisions for the equities list --the main
data structure in this application.

### Who are the Readers and Writers?
When designing a shared data structure, it is very important to understand the characteristics
of the threads reading, inserting, updating, or deleting data.

This application will consist of one reader thread and one writer thread. The reader thread
will be responsible for displaying information about eqities. The writer thread will be
responsible for gathering real time data about equities and updating their information.
We should, however, support multiple readers; this should not be too much trouble so
I will include it in the scope. I do not really care about supporting multiple writers.

The reader thread will have the following characteristics that should be taken into
account:
- It will iterate through the list to find whatever element it is looking for. Once it
  does, it will hold a reference to that element for an unspecified period of time. It
  will most likely be in the order of seconds.
- Once it is finished with an element, it will not reference it again (unless it iterates
  through the list to find it again).
This gives us an idea of how our data structure should accommodate the reader:
- It should not update an equity in place when a reader has a reference to it.
- A reader should not be blocked for a long period of time from iterating through
  the list.
The points above allude to the fact that an equity should *not* be protected by a lock.
If a reader holds a lock on an equity while a writer is trying to update it, the writer
will be blocked for an indefinite amount of time. Also, writer should *not* be given
an exclusive lock on the list for a long period of time. This may block the reader from
getting its next equity. With these factors taken into account, I think a COW (copy on write)
scheme is the best fit for the reader.

The writer thread will have the following characteristics that should be taken into
account:
- It will need to add new equities to the list; this operation will be very infrequent.
  Once the equities are added to the portfolio on startup, additions will be few and
  far between.
- It will need to delete equities from the list; this operation will also be very
  infrequent.
- It will need to update the information for an equity currently in the list. This
  will by far be the most frequent operation for the writer.
Those points (along with the reader's characteristics) give us an idea of how our
data structure should accommodate the writer:
- The writer should not block for a long period of time when attempting to update an
  equity. Time blocked for inserting/deleting is not a huge deal.
- The writer should **never** udate an element in place. In the case a reader has a
  reference, this will possibly cause the reader to read bogus data. In the case no
  reader has the reference, we may end up blocking a reader traversing the list.

### My Design Choice

#### (Data structure overview)
