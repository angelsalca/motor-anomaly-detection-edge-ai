# Third-party components

The retained project archive contains NanoEdge AI Studio generated libraries for Cortex-M33.
Those binary libraries are not redistributed in this public portfolio repository.

To reproduce the Edge AI portion, generate/install compatible NanoEdge AI libraries locally and keep them outside the public repository unless their license explicitly permits redistribution.

The recovered Portenta firmware expects headers named:

- `NanoEdgeAI_Accel.h`
- `NanoEdgeAI_Mic.h`

The data-collection sketches in `firmware/data_collection/` show the sensor frame formats used to create training/learning data.
