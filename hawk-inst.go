package hawk

/*
#include <hawk.h>
*/
import "C"
import "fmt"
import "sync"
import "weak"

type CPtr interface {
	//~*C.hawk_t | ~*C.hawk_rtx_t
	*C.hawk_t | *C.hawk_rtx_t
}

type Instance[CT CPtr, GT interface{}] struct {
	c CT // c object
	g weak.Pointer[GT] // go object
	next_free int
}

type InstanceTable[CT CPtr, GT interface{}] struct {
	name       string
	mtx        sync.Mutex
	insts      []Instance[CT, GT]
	next_free  int
}

type HawkInstance = Instance[*C.hawk_t, Hawk]
type HawkInstanceTable = InstanceTable[*C.hawk_t, Hawk]

type RtxInstance = Instance[*C.hawk_rtx_t, Rtx]
type RtxInstanceTable = InstanceTable[*C.hawk_rtx_t, Rtx]

var inst_table HawkInstanceTable = HawkInstanceTable{ name: "hawk", next_free: -1 }
var rtx_inst_table RtxInstanceTable = RtxInstanceTable{ name: "rtx", next_free: -1 }

func (itab *InstanceTable[CT, GT]) add_instance(c CT, g *GT) int {
	itab.mtx.Lock()
	defer itab.mtx.Unlock()

	if itab.next_free >= 0 {
		var slot int
		slot = itab.next_free
		if slot >= 0 { itab.next_free = itab.insts[slot].next_free }
		itab.insts[slot].c = c
		itab.insts[slot].g = weak.Make(g)
		itab.insts[slot].next_free = -1
		return slot
	} else { // no free slots
		itab.insts = append(itab.insts, Instance[CT, GT]{c: c, g: weak.Make(g), next_free: -1})
		return len(itab.insts) - 1
	}
}

func (itab *InstanceTable[CT, GT]) delete_instance(slot int) error {
	itab.mtx.Lock()
	defer itab.mtx.Unlock()

	if slot >= len(itab.insts) || slot < 0 {
		return fmt.Errorf("index %d out of range", slot)
	}
	if itab.insts[slot].next_free >= 0 {
		return fmt.Errorf("no instance at index %d", slot)
	}

	itab.insts[slot].c = CT(nil)
	itab.insts[slot].g = weak.Make((*GT)(nil)) // this may not even be necessary as it's a weak pointer
	itab.insts[slot].next_free = itab.next_free
	itab.next_free = slot

	return nil
}

func (itab *InstanceTable[CT, GT]) slot_to_instance(slot int) Instance[CT, GT] {
	itab.mtx.Lock()
	defer itab.mtx.Unlock()
	return itab.insts[slot]
}
