set(SRCS "sigma_delta.cpp"
         "sigma_delta_arithmetics.cpp"
         "linear_interpolator.cpp"
         "iir_interpolator.cpp"
         "pcm_pdm_conversion.cpp"
    )


vp_block(NAME pcm_pdm_conversion
PREFIX pcm_pdm_conversion
FORCE_BUILD 1
SOURCES ${SRCS})
