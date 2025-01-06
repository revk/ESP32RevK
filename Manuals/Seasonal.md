# Seasonal codes

The function `const char *revk_season (time_t now)` is provided to return a string of seasonal codes. Applications make use of this for seasonal information, such as displays/doorbells.

The response is a set of codes that apply, but some applications just use the first letter. When multiple letters the first letter changes one the hour.

Applications should allow for new codes being added in the future, and for some changes to time ranges.

## Codes

|Code|When|
|----|----|
|`M`|Full moon (12 hours before and after)|
|`N`|New moon (12 hours before and after)|
|`V`|Valentine's day (14th Feb)|
|`X`|Christmas time (1st to 25th Dec)|
|`Y`|New Year (1st to 7th Jan)|
|`H`|Halloween (31st Oct from 4pm to midnight)|
|`E`|Easter (Good Friday to Sunday)|
