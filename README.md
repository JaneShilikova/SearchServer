# **Search server**

Document keyword search and result sorting using TF-IDF (term frequency-inverse document frequency)

## **Features:**

* stop word processing (are not taking into account and do not influence on result)
* minus word processing (documents that consists of minus words are not included into result)
* creating and processing a request queue
* deleting duplicate documents
* paginatinon of results
* ability to work in multithreaded mode
* RemoveDocument, FindTopDocuments, MatchDocument, FindAllDocuments can be executed in sequenced or parallel mode
