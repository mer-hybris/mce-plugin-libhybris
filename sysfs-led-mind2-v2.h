/** @file sysfs-led-mind2-v2.h
 *
 * mce-plugin-libhybris - Libhybris plugin for Mode Control Entity
 * <p>
 * Copyright (c) 2024 Jollyboys Ltd.
 * <p>
 * @author Simo Piiroinen <simo.piiroinen@jolla.com>
 *
 * mce-plugin-libhybris is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * mce-plugin-libhybris is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with mce-plugin-libhybris; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef  SYSFS_LED_MIND2V2_H_
# define SYSFS_LED_MIND2V2_H_

# include "sysfs-led-main.h"

bool led_control_mind2v2_probe(led_control_t *self);

#endif /* SYSFS_LED_MIND2V2_H_ */
