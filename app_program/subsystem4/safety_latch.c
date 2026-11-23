/*
 *  safety_latch.c
 *  Safety's fault latch and RESET handshake.  See safety_latch.h.
 */

#include <stddef.h>

#include "imu_msgs.h"
#include "safety_latch.h"

/* Nothing latched, no reset waiting. */
void
safety_latch_init (safety_latch_t * p_latch)
{
    if (NULL != p_latch)
    {
        p_latch->b_latched       = false;
        p_latch->b_reset_pending = false;
        p_latch->cause           = 0u;
        p_latch->source          = 0u;
        p_latch->detail          = 0u;
        p_latch->mask            = 0u;
    }
}

/*
 * A fault report (fault_evt_t's fields).  A status cause sets its mask
 * bit, or clears it when b_cleared, and nothing else.  Any other cause
 * sets its bit and latches when nothing is latched; while a reset waits
 * for Motion, it cancels the reset and becomes the latched cause (G20);
 * otherwise it is only counted (the caller counts every latching
 * report).  b_cleared is ignored for a latching cause.
 */
safety_latch_out_t
safety_latch_report (safety_latch_t * p_latch, uint8_t cause,
                     uint8_t source, uint16_t detail, bool b_cleared)
{
    safety_latch_out_t out = SAFETY_LATCH_COUNTED;

    if (NULL == p_latch)
    {
        /* Nothing to update. */
    }
    else if (!fault_cause_latches(cause))
    {
        uint16_t const bit = fault_cause_bit(cause);

        p_latch->mask = b_cleared ? (uint16_t) (p_latch->mask & ~bit)
                                  : (uint16_t) (p_latch->mask | bit);
        out = SAFETY_LATCH_STATUS;
    }
    else
    {
        /* 0 for a cause outside the table: latched all the same. */
        p_latch->mask |= fault_cause_bit(cause);

        if (!p_latch->b_latched || p_latch->b_reset_pending)
        {
            out = p_latch->b_latched ? SAFETY_LATCH_RESET_CANCELLED
                                     : SAFETY_LATCH_LATCHED;
            p_latch->b_latched       = true;
            p_latch->b_reset_pending = false;
            p_latch->cause           = cause;
            p_latch->source          = source;
            p_latch->detail          = detail;
        }
        else
        {
            /* Already latched, no reset waiting: the first cause stays. */
        }
    }

    return (out);
}

/*
 * RESET pressed while a fault is latched.  Returns true when the reset
 * starts (ask Motion to zero its state, then wait for its
 * acknowledgement): a fault is latched, no reset is waiting already and
 * its cause has cleared.
 */
bool
safety_latch_reset (safety_latch_t * p_latch, bool b_cause_cleared)
{
    bool b_start = false;

    if ((NULL != p_latch) && p_latch->b_latched &&
        !p_latch->b_reset_pending && b_cause_cleared)
    {
        p_latch->b_reset_pending = true;
        b_start = true;
    }

    return (b_start);
}

/*
 * Motion acknowledged zeroing its state.  Returns true when that
 * releases the latch: only while a reset is waiting.  The release
 * clears the latching causes' bits; the status bits stay as they are.
 */
bool
safety_latch_ack (safety_latch_t * p_latch)
{
    bool b_release = false;

    if ((NULL != p_latch) && p_latch->b_latched && p_latch->b_reset_pending)
    {
        p_latch->b_latched       = false;
        p_latch->b_reset_pending = false;
        p_latch->mask           &= (uint16_t) FAULT_MASK_STATUS;
        b_release = true;
    }

    return (b_release);
}

/* No acknowledgement in time: the reset is abandoned, the latch stays. */
void
safety_latch_ack_timeout (safety_latch_t * p_latch)
{
    if (NULL != p_latch)
    {
        p_latch->b_reset_pending = false;
    }
}

/*** end of file ***/
