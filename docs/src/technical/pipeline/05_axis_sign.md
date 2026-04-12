# Axis Sign Modifiers

neg = avg(max(0, -axis_i))  
harmony = 1 - neg

corruption += K_SIGN_CORRUPTION · neg  
purity     -= K_SIGN_PURITY · neg  
potency    += K_SIGN_POTENCY · harmony  
stability  += K_SIGN_STABILITY · harmony  

C_bs = max(0, -body·spirit)^p  
C_mn = max(0, -mind·nature)^p  

corruption += K_CONFLICT_CORRUPTION · C_bs  
stability  -= K_CONFLICT_STABILITY · C_mn