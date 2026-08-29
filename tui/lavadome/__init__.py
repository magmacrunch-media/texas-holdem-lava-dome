"""Texas Hold'Em Lava Dome — terminal version.

A third sibling to ``web/`` (adenosine, browser) and ``wii/`` (magnolia, C99),
built on the `magmacrunch.engine` terminal backend.

Solo Texas Hold'Em with no opponent. The dome charges an escalating ante each
round and scores your hand against a threshold that climbs with it: beat the
threshold and win chips, miss it and forfeit the bet. After each round you
choose to bank chips — safe, and the banked total is your score — or leave them
in play. The session ends when you go bust, or when you escape voluntarily.
"""

__version__ = "0.1.0"
